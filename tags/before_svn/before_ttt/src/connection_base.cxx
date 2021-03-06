/*-------------------------------------------------------------------------
 *
 *   FILE
 *	connection_base.cxx
 *
 *   DESCRIPTION
 *      implementation of the pqxx::Connection_base abstract base class.
 *   pqxx::Connection_base encapsulates a frontend to backend connection
 *
 * Copyright (c) 2001-2003, Jeroen T. Vermeulen <jtv@xs4all.nl>
 *
 *-------------------------------------------------------------------------
 */
#include <algorithm>
#include <cstdio>
#include <stdexcept>

#include "pqxx/connection_base.h"
#include "pqxx/result.h"
#include "pqxx/transaction.h"
#include "pqxx/trigger.h"


using namespace PGSTD;


extern "C"
{
// Pass C-linkage notice processor call on to C++-linkage Noticer object.  The
// void * argument points to the Noticer.
void pqxxNoticeCaller(void *arg, const char *Msg)
{
  if (arg && Msg) (*static_cast<pqxx::Noticer *>(arg))(Msg);
}
}


pqxx::Connection_base::Connection_base(const string &ConnInfo) :
  m_ConnInfo(ConnInfo),
  m_Conn(0),
  m_Trans(),
  m_Noticer(),
  m_Trace(0)
{
}


pqxx::Connection_base::Connection_base(const char ConnInfo[]) :
  m_ConnInfo(ConnInfo ? ConnInfo : ""),
  m_Conn(0),
  m_Trans(),
  m_Noticer(),
  m_Trace(0)
{
}


void pqxx::Connection_base::Connect()
{
  if (m_Conn) throw logic_error("libqxx internal error: spurious Connect()");

  m_Conn = PQconnectdb(m_ConnInfo.c_str());

  if (!m_Conn)
    throw broken_connection();

  if (!is_open())
  {
    const string Msg( ErrMsg() );
    Disconnect();
    throw broken_connection(Msg);
  }

  if (Status() != CONNECTION_OK)
  {
    const string Msg( ErrMsg() );
    Disconnect();
    throw runtime_error(Msg);
  }

  SetupState();
}



void pqxx::Connection_base::Deactivate()
{
  if (m_Conn)
  {
    if (m_Trans.get())
      throw logic_error("Attempt to deactivate connection while transaction "
	                "'" + m_Trans.get()->Name() + "' "
			"still open");

    Disconnect();
  }
}


void pqxx::Connection_base::SetVariable(const PGSTD::string &Var,
                                      const PGSTD::string &Value)
{
  if (m_Trans.get())
  {
    m_Trans.get()->SetVariable(Var, Value);
  }
  else
  {
    RawSetVar(Var, Value);
    m_Vars[Var] = Value;
  }
}


/** Set up various parts of logical connection state that may need to be
 * recovered because the physical connection to the database was lost and is
 * being reset, or that may not have been initialized yet.
 */
void pqxx::Connection_base::SetupState()
{
  if (!m_Conn) 
    throw logic_error("libpqxx internal error: SetupState() on no connection");

  if (m_Noticer.get())
    PQsetNoticeProcessor(m_Conn, pqxxNoticeCaller, m_Noticer.get());

  InternalSetTrace();

  // Reinstate all active triggers
  if (!m_Triggers.empty())
  {
    const TriggerList::const_iterator End = m_Triggers.end();
    string Last;
    for (TriggerList::const_iterator i = m_Triggers.begin(); i != End; ++i)
    {
      // m_Triggers can handle multiple Triggers waiting on the same event; 
      // issue just one LISTEN for each event.
      if (i->first != Last)
      {
	const string LQ("LISTEN " + i->first);
        Result R( PQexec(m_Conn, LQ.c_str()) );
        R.CheckStatus(LQ);
        Last = i->first;
      }
    }
  }

  for (map<string,string>::const_iterator i=m_Vars.begin(); 
       i!=m_Vars.end(); 
       ++i)
    RawSetVar(i->first, i->second);
}


void pqxx::Connection_base::Disconnect() throw ()
{
  if (m_Conn)
  {
    PQfinish(m_Conn);
    m_Conn = 0;
  }
}


bool pqxx::Connection_base::is_open() const
{
  return m_Conn && (Status() != CONNECTION_BAD);
}


PGSTD::auto_ptr<pqxx::Noticer> 
pqxx::Connection_base::SetNoticer(PGSTD::auto_ptr<Noticer> N)
{
  if (N.get()) PQsetNoticeProcessor(m_Conn, pqxxNoticeCaller, N.get());
  else PQsetNoticeProcessor(m_Conn, 0, 0);
  
  auto_ptr<Noticer> Old = m_Noticer;
  // TODO: Can this line fail?  If yes, we'd be killing Old prematurely...
  m_Noticer = N;

  return Old;
}


void pqxx::Connection_base::ProcessNotice(const char msg[]) throw ()
{
  if (msg)
  {
    // TODO: Find cleaner solution for default case!
    if (m_Noticer.get()) (*m_Noticer.get())(msg);
    else fputs(msg, stderr);
  }
}


void pqxx::Connection_base::Trace(FILE *Out)
{
  m_Trace = Out;
  if (m_Conn) InternalSetTrace();
}


void pqxx::Connection_base::AddTrigger(pqxx::Trigger *T)
{
  if (!T) throw invalid_argument("Null trigger registered");

  // Add to triggers list and attempt to start listening.
  const TriggerList::iterator p = m_Triggers.find(T->Name());
  const TriggerList::value_type NewVal(T->Name(), T);

  if (m_Conn && (p == m_Triggers.end()))
  {
    // Not listening on this event yet, start doing so.
    const string LQ("LISTEN " + string(T->Name())); 
    Result R( PQexec(m_Conn, LQ.c_str()) );

    try
    {
      R.CheckStatus(LQ);
    }
    catch (const broken_connection &)
    {
    }
    catch (const exception &)
    {
      if (is_open()) throw;
    }
    m_Triggers.insert(NewVal);
  }
  else
  {
    m_Triggers.insert(p, NewVal);
  }

}


void pqxx::Connection_base::RemoveTrigger(pqxx::Trigger *T) throw ()
{
  if (!T) return;

  try
  {
    // Keep Sun compiler happy...  Hope it doesn't annoy other compilers
    pair<const string, Trigger *> tmp_pair(T->Name(), T);
    TriggerList::value_type E = tmp_pair;

    typedef pair<TriggerList::iterator, TriggerList::iterator> Range;
    Range R = m_Triggers.equal_range(E.first);

    const TriggerList::iterator i = find(R.first, R.second, E);

    if (i == R.second) 
    {
      ProcessNotice("Attempt to remove unknown trigger '" + 
		    E.first + 
		    "'");
    }
    else
    {
      if (m_Conn && (R.second == ++R.first))
	PQexec(m_Conn, ("UNLISTEN " + string(T->Name())).c_str());

      m_Triggers.erase(i);
    }
  }
  catch (const exception &e)
  {
    ProcessNotice(e.what());
  }
}


void pqxx::Connection_base::GetNotifs()
{
  if (!m_Conn) return;

  PQconsumeInput(m_Conn);

  // Even if somehow we receive notifications during our transaction, don't
  // deliver them.
  if (m_Trans.get()) return;

  for (PQAlloc<PGnotify> N( PQnotifies(m_Conn) ); N; N = PQnotifies(m_Conn))
  {
    typedef TriggerList::iterator TI;

    pair<TI, TI> Hit = m_Triggers.equal_range(string(N->relname));
    for (TI i = Hit.first; i != Hit.second; ++i)
      try
      {
        (*i->second)(N->be_pid);
      }
      catch (const exception &e)
      {
	ProcessNotice("Exception in trigger handler '" +
		      i->first + 
		      "': " + 
		      e.what() +
		      "\n");
      }

    N.close();
  }
}


const char *pqxx::Connection_base::ErrMsg() const
{
  return m_Conn ? PQerrorMessage(m_Conn) : "No connection to database";
}


pqxx::Result pqxx::Connection_base::Exec(const char Query[], 
                                       int Retries, 
				       const char OnReconnect[])
{
  Activate();

  Result R( PQexec(m_Conn, Query) );

  while ((Retries > 0) && !R && !is_open())
  {
    Retries--;

    Reset(OnReconnect);
    if (is_open()) R = PQexec(m_Conn, Query);
  }

  if (!R) throw broken_connection();
  R.CheckStatus(Query);

  GetNotifs();

  return R;
}


void pqxx::Connection_base::Reset(const char OnReconnect[])
{
  // Attempt to set up or restore connection
  if (!m_Conn) Connect();
  else 
  {
    PQreset(m_Conn);
    SetupState();

    // Perform any extra patchup work involved in restoring the connection,
    // typically set up a transaction.
    if (OnReconnect)
    {
      Result Temp( PQexec(m_Conn, OnReconnect) );
      Temp.CheckStatus(OnReconnect);
    }
  }
}


void pqxx::Connection_base::close() throw ()
{
  try
  {
    if (m_Trans.get()) 
      ProcessNotice("Closing connection while transaction '" +
		    m_Trans.get()->Name() +
		    "' still open\n");

    if (!m_Triggers.empty())
    {
      string T;
      for (TriggerList::const_iterator i = m_Triggers.begin(); 
	   i != m_Triggers.end();
	   ++i)
	T += " " + i->first;

        ProcessNotice("Closing connection with outstanding triggers:" + 
	              T + 
		      "\n");
      m_Triggers.clear();
    }

    Disconnect();
  }
  catch (...)
  {
  }
}


void pqxx::Connection_base::RawSetVar(const string &Var, const string &Value)
{
    Activate();
    Exec(("SET " + Var + "=" + Value).c_str(), 0);
}


void pqxx::Connection_base::AddVariables(const map<string,string> &Vars)
{
  for (map<string,string>::const_iterator i=Vars.begin(); i!=Vars.end(); ++i)
    m_Vars[i->first] = i->second;
}


void pqxx::Connection_base::InternalSetTrace()
{
  if (m_Trace) PQtrace(m_Conn, m_Trace);
  else PQuntrace(m_Conn);
}



void pqxx::Connection_base::RegisterTransaction(Transaction_base *T)
{
  m_Trans.Register(T);
}


void pqxx::Connection_base::UnregisterTransaction(Transaction_base *T) 
  throw ()
{
  try
  {
    m_Trans.Unregister(T);
  }
  catch (const exception &e)
  {
    ProcessNotice(string(e.what()) + "\n");
  }
}


void pqxx::Connection_base::MakeEmpty(pqxx::Result &R, ExecStatusType Stat)
{
  if (!m_Conn) 
    throw logic_error("Internal libpqxx error: MakeEmpty() on null connection");

  R = Result(PQmakeEmptyPGresult(m_Conn, Stat));
}


void pqxx::Connection_base::BeginCopyRead(const string &Table)
{
  const string CQ("COPY " + Table + " TO STDOUT");
  Result R( Exec(CQ.c_str()) );
  R.CheckStatus(CQ);
}


bool pqxx::Connection_base::ReadCopyLine(string &Line)
{
  if (!m_Conn)
    throw logic_error("Internal libpqxx error: "
	              "ReadCopyLine() on null connection");

  char Buf[256];
  bool LineComplete = false;

  Line.erase();

  while (!LineComplete)
  {
    switch (PQgetline(m_Conn, Buf, sizeof(Buf)))
    {
    case EOF:
      throw runtime_error("Unexpected EOF from backend");

    case 0:
      LineComplete = true;
      break;

    case 1:
      break;

    default:
      throw runtime_error("Unexpected COPY response from backend");
    }

    Line += Buf;
  }

  bool Result = (Line != "\\.");

  if (!Result) Line.erase();

  return Result;
}


void pqxx::Connection_base::BeginCopyWrite(const string &Table)
{
  const string CQ("COPY " + Table + " FROM STDIN");
  Result R( Exec(CQ.c_str()) );
  R.CheckStatus(CQ);
}



void pqxx::Connection_base::WriteCopyLine(const string &Line)
{
  if (!m_Conn)
    throw logic_error("Internal libpqxx error: "
	              "WriteCopyLine() on null connection");

  PQputline(m_Conn, (Line + "\n").c_str());
}


// End COPY operation.  Careful: this assumes that no more lines remain to be
// read or written, and the COPY operation has been properly terminated with a
// line containing only the two characters "\."
void pqxx::Connection_base::EndCopy()
{
  if (PQendcopy(m_Conn) != 0) throw runtime_error(ErrMsg());
}



