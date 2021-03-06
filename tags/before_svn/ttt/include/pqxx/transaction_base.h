/*-------------------------------------------------------------------------
 *
 *   FILE
 *	pqxx/transaction_base.h
 *
 *   DESCRIPTION
 *      common code and definitions for the transaction classes.
 *   pqxx::transaction_base defines the interface for any abstract class that
 *   represents a database transaction
 *
 * Copyright (c) 2001-2003, Jeroen T. Vermeulen <jtv@xs4all.nl>
 *
 * See COPYING for copyright license.  If you did not receive a file called
 * COPYING with this source code, please notify the distributor of this mistake,
 * or contact the author.
 *
 *-------------------------------------------------------------------------
 */
#ifndef PQXX_TRANSACTION_BASE_H
#define PQXX_TRANSACTION_BASE_H


/* End-user programs need not include this file, unless they define their own
 * transaction classes.  This is not something the typical program should want
 * to do.
 *
 * However, reading this file is worthwhile because it defines the public
 * interface for the available transaction classes such as transaction and 
 * nontransaction.
 */

#include "pqxx/connection_base.h"
#include "pqxx/isolation.h"
#include "pqxx/result.h"

/* Methods tested in eg. self-test program test1 are marked with "//[t1]"
 */


namespace pqxx
{
class connection_base; 	// See pqxx/connection_base.h
class result; 		// See pqxx/result.h
class tablestream;	// See pqxx/tablestream.h


/// User-readable class name for use by unique
template<> inline PGSTD::string Classname(const tablestream *) 
{ 
  return "tablestream"; 
}


/// Interface definition (and common code) for "transaction" classes.  
/** All database access must be channeled through one of these classes for 
 * safety, although not all implementations of this interface need to provide 
 * full transactional integrity.
 *
 * Several implementations of this interface are shipped with libpqxx, including
 * the plain transaction class, the entirely unprotected nontransaction, and the
 * more cautions robusttransaction.
 */
class PQXX_LIBEXPORT transaction_base
{
  // TODO: Move commit policy (robust, default, normal) out of inheritance tree
  // TODO: Support read-only transactions (no retry, not for nontransaction)
  // TODO: Retry non-serializable transaction w/update only on broken_connection
public:
  /// If nothing else is known, our isolation level is at least read_committed
  typedef isolation_traits<read_committed> isolation_tag;

  virtual ~transaction_base() =0;					//[t1]

  /// Commit the transaction
  /** Unless this function is called explicitly, the transaction will not be
   * committed (actually the nontransaction implementation breaks this rule,
   * hence the name).
   *
   * Once this function returns, the whole transaction will typically be
   * irrevocably completed in the database.  There is also, however, a minute
   * risk that the connection to the database may be lost at just the wrong
   * moment.  In that case, libpqxx may be unable to determine whether the
   * transaction was completed or aborted and an in_doubt_error will be thrown
   * to make this fact known to the caller.  The robusttransaction 
   * implementation takes some special precautions to reduce this risk.
   */
  void Commit();							//[t1]

  /// Abort the transaction
  /** No special effort is required to call this function; it will be called
   * implicitly when the transaction is destructed.
   */
  void Abort();								//[t10]

  /// Execute query
  /** Perform a query in this transaction.
   * @param Query the query or command to execute
   * @param Desc optional identifier for query, to help pinpoint SQL errors
   */
  result Exec(const char Query[], 
      	      const PGSTD::string &Desc=PGSTD::string());		//[t1]

  /// Execute query
  /** Perform a query in this transaction.  This version may be slightly
   * slower than the version taking a const char[], although the difference is
   * not likely to be very noticeable compared to the time required to execute
   * even a simple query.
   * @param Query the query or command to execute
   * @param Desc optional identifier for query, to help pinpoint SQL errors
   */
  result Exec(const PGSTD::string &Query,
              const PGSTD::string &Desc=PGSTD::string()) 		//[t2]
  	{ return Exec(Query.c_str(), Desc); }

  /// Have connection process warning message
  void ProcessNotice(const char Msg[]) const 				//[t14]
  	{ m_Conn.ProcessNotice(Msg); }
  /// Have connection process warning message
  void ProcessNotice(const PGSTD::string &Msg) const			//[t14]
  	{ m_Conn.ProcessNotice(Msg); }

  PGSTD::string Name() const { return m_Name; }				//[t1]

  /// Connection this transaction is running in
  connection_base &Conn() const { return m_Conn; }			//[t4]

  /// Set session variable in this connection
  /** The new value is typically forgotten if the transaction aborts.  
   * Known exceptions to this rule are nontransaction, and PostgreSQL versions
   * prior to 7.3.  In the case of nontransaction, the set value will be kept
   * regardless, but if the connection ever needs to be recovered, the set value
   * will not be restored.
   * @param Var the variable to set
   * @param Value the value to store in the variable
   */
  void SetVariable(const PGSTD::string &Var, const PGSTD::string &Value);//[]


protected:
  /// Create a transaction.  The optional name, if given, must begin with a
  /// letter and may contain letters and digits only.
  explicit transaction_base(connection_base &, 
		          const PGSTD::string &TName=PGSTD::string());

  /// Begin transaction.  To be called by implementing class, typically from 
  /// constructor.
  void Begin();

  /// End transaction.  To be called by implementing class' destructor 
  void End() throw ();

  /// To be implemented by derived implementation class: start transaction
  virtual void DoBegin() =0;
  /// To be implemented by derived implementation class: perform query
  virtual result DoExec(const char Query[]) =0;
  /// To be implemented by derived implementation class: commit transaction
  virtual void DoCommit() =0;
  /// To be implemented by derived implementation class: abort transaction
  virtual void DoAbort() =0;

  // For use by implementing class:

  /// Execute query on connection directly
  result DirectExec(const char C[], int Retries, const char OnReconnect[]);
 
private:
  /* A transaction goes through the following stages in its lifecycle:
   *  - nascent: the transaction hasn't actually begun yet.  If our connection 
   *    fails at this stage, it may recover and the transaction can attempt to
   *    establish itself again.
   *  - active: the transaction has begun.  Since no commit command has been 
   *    issued, abortion is implicit if the connection fails now.
   *  - aborted: an abort has been issued; the transaction is terminated and 
   *    its changes to the database rolled back.  It will accept no further 
   *    commands.
   *  - committed: the transaction has completed successfully, meaning that a 
   *    commit has been issued.  No further commands are accepted.
   *  - in_doubt: the connection was lost at the exact wrong time, and there is
   *    no way of telling whether the transaction was committed or aborted.
   *
   * Checking and maintaining state machine logic is the responsibility of the 
   * base class (ie., this one).
   */
  enum Status 
  { 
    st_nascent, 
    st_active, 
    st_aborted, 
    st_committed,
    st_in_doubt
  };


  friend class Cursor;
  int GetUniqueCursorNum() { return m_UniqueCursorNum++; }
  void MakeEmpty(result &R) const { m_Conn.MakeEmpty(R); }

  friend class tablestream;
  void RegisterStream(tablestream *);
  void UnregisterStream(tablestream *) throw ();
  void EndCopy() { m_Conn.EndCopy(); }
  friend class tablereader;
  void BeginCopyRead(const PGSTD::string &Table) 
  	{ m_Conn.BeginCopyRead(Table); }
  bool ReadCopyLine(PGSTD::string &L) { return m_Conn.ReadCopyLine(L); }
  friend class tablewriter;
  void BeginCopyWrite(const PGSTD::string &Table) 
  	{ m_Conn.BeginCopyWrite(Table); }
  void WriteCopyLine(const PGSTD::string &L) { m_Conn.WriteCopyLine(L); }

  connection_base &m_Conn;

  PGSTD::string m_Name;
  int m_UniqueCursorNum;
  unique<tablestream> m_Stream;
  Status m_Status;
  bool m_Registered;
  mutable PGSTD::map<PGSTD::string, PGSTD::string> m_Vars;

  // Not allowed:
  transaction_base();
  transaction_base(const transaction_base &);
  transaction_base &operator=(const transaction_base &);
};


}

#endif

