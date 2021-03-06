/*-------------------------------------------------------------------------
 *
 *   FILE
 *	tablewriter.cxx
 *
 *   DESCRIPTION
 *      implementation of the pqxx::tablewriter class.
 *   pqxx::tablewriter enables optimized batch updates to a database table
 *
 * Copyright (c) 2001-2013, Jeroen T. Vermeulen <jtv@xs4all.nl>
 *
 * See COPYING for copyright license.  If you did not receive a file called
 * COPYING with this source code, please notify the distributor of this mistake,
 * or contact the author.
 *
 *-------------------------------------------------------------------------
 */
#include "pqxx/compiler-internal.hxx"

#include "pqxx/tablereader"
#include "pqxx/tablewriter"
#include "pqxx/transaction"

using namespace PGSTD;
using namespace pqxx::internal;


pqxx::tablewriter::tablewriter(transaction_base &T,
    const PGSTD::string &WName,
    const PGSTD::string &Null) :
  namedclass("tablewriter", WName),
  tablestream(T, Null)
{
  setup(T, WName);
}


pqxx::tablewriter::~tablewriter() throw ()
{
#ifdef PQXX_QUIET_DESTRUCTORS
  disable_noticer Quiet(m_Trans.conn());
#endif
  try
  {
    writer_close();
  }
  catch (const exception &e)
  {
    reg_pending_error(e.what());
  }
}


void pqxx::tablewriter::setup(transaction_base &T,
    const PGSTD::string &WName,
    const PGSTD::string &Columns)
{
  T.BeginCopyWrite(WName, Columns);
  register_me();
}


pqxx::tablewriter &pqxx::tablewriter::operator<<(pqxx::tablereader &R)
{
  string Line;
  // TODO: Can we do this in binary mode? (Might require protocol version check)
  while (R.get_raw_line(Line)) write_raw_line(Line);
  return *this;
}


void pqxx::tablewriter::write_raw_line(const PGSTD::string &Line)
{
  const string::size_type len = Line.size();
  m_Trans.WriteCopyLine(
	(!len || Line[len-1] != '\n') ?
	Line :
	string(Line, 0, len-1));
}


void pqxx::tablewriter::complete()
{
  writer_close();
}


void pqxx::tablewriter::writer_close()
{
  if (!is_finished())
  {
    base_close();
    try
    {
      m_Trans.EndCopyWrite();
    }
    catch (const exception &)
    {
      try { base_close(); } catch (const exception &) {}
      throw;
    }
  }
}


namespace
{
inline char escapechar(char i)
{
  char r = '\0';
  switch (i)
  {
    case 8:	r='b';	break;	// backspace
    case 11:	r='v';	break;	// vertical tab
    case 12:	r='f';	break;	// form feed
    case '\n':	r='n';	break;	// newline
    case '\t':	r='t';	break;	// tab
    case '\r':	r='r';	break;	// carriage return
    case '\\':	r='\\';	break;	// backslash
  }
  return r;
}

inline bool unprintable(char i)
{
  return i < ' ' || i > '~';
}

inline char tooctdigit(unsigned int i, int n)
{
  return number_to_digit((i>>(3*n)) & 0x07);
}
} // namespace


string pqxx::internal::Escape(const string &s, const string &null)
{
  if (s == null) return "\\N";
  if (s.empty()) return s;

  string R;
  R.reserve(s.size()+1);

  const string::const_iterator s_end(s.end());
  for (string::const_iterator j = s.begin(); j != s_end; ++j)
  {
    const char c = *j;
    const char e = escapechar(c);
    if (e)
    {
      R += '\\';
      R += e;
    }
    else if (unprintable(c))
    {
      R += "\\\\";
      unsigned char u = static_cast<unsigned char>(c);
      for (int n=2; n>=0; --n) R += tooctdigit(u,n);
    }
    else
    {
      R += c;
    }
  }
  return R;
}


