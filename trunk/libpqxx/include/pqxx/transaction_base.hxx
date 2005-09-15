/*-------------------------------------------------------------------------
 *
 *   FILE
 *	pqxx/transaction_base.hxx
 *
 *   DESCRIPTION
 *      common code and definitions for the transaction classes.
 *   pqxx::transaction_base defines the interface for any abstract class that
 *   represents a database transaction
 *   DO NOT INCLUDE THIS FILE DIRECTLY; include pqxx/transaction_base instead.
 *
 * Copyright (c) 2001-2005, Jeroen T. Vermeulen <jtv@xs4all.nl>
 *
 * See COPYING for copyright license.  If you did not receive a file called
 * COPYING with this source code, please notify the distributor of this mistake,
 * or contact the author.
 *
 *-------------------------------------------------------------------------
 */
#include "pqxx/libcompiler.h"

/* End-user programs need not include this file, unless they define their own
 * transaction classes.  This is not something the typical program should want
 * to do.
 *
 * However, reading this file is worthwhile because it defines the public
 * interface for the available transaction classes such as transaction and
 * nontransaction.
 */

#include "pqxx/connection_base"
#include "pqxx/isolation"
#include "pqxx/result"

/* Methods tested in eg. self-test program test001 are marked with "//[t1]"
 */

// TODO: Any way the BEGIN and the first query can be concatenated/pipelined?

namespace pqxx
{
class connection_base;
class transaction_base;


namespace internal
{
class PQXX_LIBEXPORT transactionfocus : public namedclass
{
public:
  transactionfocus(transaction_base &t,
      const PGSTD::string &Name,
      const PGSTD::string &Classname) :
    namedclass(Name, Classname),
    m_Trans(t),
    m_registered(false)
  {
  }

protected:
  void register_me();
  void unregister_me() throw ();
  void reg_pending_error(const PGSTD::string &) throw ();
  bool registered() const throw () { return m_registered; }

  transaction_base &m_Trans;

private:
  bool m_registered;

  /// Not allowed
  transactionfocus();
  /// Not allowed
  transactionfocus(const transactionfocus &);
  /// Not allowed
  transactionfocus &operator=(const transactionfocus &);
};
} // namespace internal



/// Interface definition (and common code) for "transaction" classes.
/** 
 * @addtogroup transaction Transaction classes
 * All database access must be channeled through one of these classes for
 * safety, although not all implementations of this interface need to provide
 * full transactional integrity.
 *
 * Several implementations of this interface are shipped with libpqxx, including
 * the plain transaction class, the entirely unprotected nontransaction, and the
 * more cautions robusttransaction.
 */
class PQXX_LIBEXPORT transaction_base : public internal::namedclass
{
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
  void commit();							//[t1]

  /// Abort the transaction
  /** No special effort is required to call this function; it will be called
   * implicitly when the transaction is destructed.
   */
  void abort();								//[t10]

  /// Execute query
  /** Perform a query in this transaction.
   * @param Query Query or command to execute
   * @param Desc Optional identifier for query, to help pinpoint SQL errors
   * @return A result set describing the query's or command's result
   */
  result exec(const char Query[],
      	      const PGSTD::string &Desc=PGSTD::string());		//[t1]

  /// Execute query
  /** Perform a query in this transaction.  This version may be slightly
   * slower than the version taking a const char[], although the difference is
   * not likely to be very noticeable compared to the time required to execute
   * even a simple query.
   * @param Query Query or command to execute
   * @param Desc Optional identifier for query, to help pinpoint SQL errors
   * @return A result set describing the query's or command's result
   */
  result exec(const PGSTD::string &Query,
              const PGSTD::string &Desc=PGSTD::string()) 		//[t2]
  	{ return exec(Query.c_str(), Desc); }

  result exec(const PGSTD::stringstream &Query,
      	      const PGSTD::string &Desc=PGSTD::string())		//[t9]
    	{ return exec(Query.str(), Desc); }

  /**
   * @name Prepared statements
   */
  //@{
  /// Execute parameterless prepared statement
  /** Prepared statements are defined using the connection classes' prepare()
   * functions, and continue to live on in the ongoing session regardless of
   * the context they were defined in (unless explicitly dropped using the
   * connection's unprepare() function).  Their execution however, like other
   * forms of query execution, requires a transaction object.
   *
   * @warning Do not try to execute a prepared statement manually through direct
   * SQL statements.  This is likely not to work, and even if it does, is likely
   * to be slower than using the proper libpqxx functions.
   *
   * @warning Actual definition of the prepared statement on the backend may be
   * deferred until its first use, which means that any errors in the prepared
   * statement may not show up until it is executed--and perhaps abort the
   * ongoing transaction in the process.
   */
  result exec_prepared(const PGSTD::string &qname)			//[t85]
	{ return m_Conn.pq_exec_prepared(qname.c_str(), 0, 0); }

  /// Execute parameterless prepared statement
  /** Prepared statements are defined using the connection classes' prepare()
   * functions, and continue to live on in the ongoing session regardless of
   * the context they were defined in (unless explicitly dropped using the
   * connection's unprepare() function).  Their execution however, like other
   * forms of query execution, requires a transaction object.
   *
   * @warning Do not try to execute a prepared statement manually through direct
   * SQL statements.  This is likely not to work, and even if it does, is likely
   * to be slower than using the proper libpqxx functions.
   *
   * @warning Actual definition of the prepared statement on the backend may be
   * deferred until its first use, which means that any errors in the prepared
   * statement may not show up until it is executed--and perhaps abort the
   * ongoing transaction in the process.
   */
  result exec_prepared(const char qname[])				//[t85]
	{ return m_Conn.pq_exec_prepared(qname, 0, 0); }

  /// Execute prepared statement with given sequence of arguments
  /** Prepared statements are defined using the connection classes' prepare()
   * functions, and continue to live on in the ongoing session regardless of
   * the context they were defined in (unless explicitly dropped using the
   * connection's unprepare() function).  Their execution however, like other
   * forms of query execution, requires a transaction object.
   *
   * To include null parameters, pass a sequence of C-style string pointers.  A
   * NULL pointer will be used as a null parameter.
   *
   * @warning Do not try to execute a prepared statement manually through direct
   * SQL statements.  This is likely not to work, and even if it does, is likely
   * to be slower than using the proper libpqxx functions.
   *
   * @warning Actual definition of the prepared statement on the backend may be
   * deferred until its first use, which means that any errors in the prepared
   * statement may not show up until it is executed--and perhaps abort the
   * ongoing transaction in the process.
   */
  template<typename STRING, typename ITER>
    result exec_prepared(STRING qname, ITER beginargs, ITER endargs)	//[t85]
  {
    if (beginargs == endargs) return exec_prepared(qname);

    typedef PGSTD::vector<PGSTD::string> pvec;
    pvec p;
    PGSTD::vector<bool> nulls;
    for (; beginargs!=endargs; ++beginargs)
    {
      const bool isnull = parm_is_null(*beginargs);
      nulls.push_back(isnull);
      p.push_back(to_string(isnull ? "" : *beginargs));
    }
    result r;
    const internal::scoped_array<const char *> pindex(p.size()+1);
    const pvec::size_type stop = p.size();
    for (pvec::size_type i=0; i < stop; ++i)
      pindex[i] = (nulls[i] ? 0 : p[i].c_str());
    pindex[stop] = 0;
    r = m_Conn.pq_exec_prepared(qname, p.size(), pindex.c_ptr());
    return r;
  }

  /// Execute prepared statement, taking arguments from given container
  /** Prepared statements are defined using the connection classes' prepare()
   * functions, and continue to live on in the ongoing session regardless of
   * the context they were defined in (unless explicitly dropped using the
   * connection's unprepare() function).  Their execution however, like other
   * forms of query execution, requires a transaction object.
   *
   * @warning Do not try to execute a prepared statement manually through direct
   * SQL statements.  This is likely not to work, and even if it does, is likely
   * to be slower than using the proper libpqxx functions.
   *
   * @warning Actual definition of the prepared statement on the backend may be
   * deferred until its first use, which means that any errors in the prepared
   * statement may not show up until it is executed--and perhaps abort the
   * ongoing transaction in the process.
   */
  template<typename CNTNR> result exec_prepared(const char qname[],	//[t85]
      const CNTNR &args)
  	{ return exec_prepared(qname, args.begin(), args.end()); }

  /// Execute prepared statement, taking arguments from given container
  /** Prepared statements are defined using the connection classes' prepare()
   * functions, and continue to live on in the ongoing session regardless of
   * the context they were defined in (unless explicitly dropped using the
   * connection's unprepare() function).  Their execution however, like other
   * forms of query execution, requires a transaction object.
   *
   * @warning Do not try to execute a prepared statement manually through direct
   * SQL statements.  This is likely not to work, and even if it does, is likely
   * to be slower than using the proper libpqxx functions.
   *
   * @warning Actual definition of the prepared statement on the backend may be
   * deferred until its first use, which means that any errors in the prepared
   * statement may not show up until it is executed--and perhaps abort the
   * ongoing transaction in the process.
   */
  template<typename CNTNR>
    result exec_prepared(const PGSTD::string &qname, CNTNR args)	//[t85]
  	{ return exec_prepared(qname, args.begin(), args.end()); }
  //@}

  /**
   * @name Error/warning output
   */
  //@{
  /// Have connection process warning message
  void process_notice(const char Msg[]) const 				//[t14]
  	{ m_Conn.process_notice(Msg); }
  /// Have connection process warning message
  void process_notice(const PGSTD::string &Msg) const			//[t14]
  	{ m_Conn.process_notice(Msg); }
  //@}

  /// Connection this transaction is running in
  connection_base &conn() const { return m_Conn; }			//[t4]

  /// Set session variable in this connection
  /** The new value is typically forgotten if the transaction aborts.
   * Known exceptions to this rule are nontransaction, and PostgreSQL versions
   * prior to 7.3.  In the case of nontransaction, the set value will be kept
   * regardless; but in that case, if the connection ever needs to be recovered,
   * the set value will not be restored.
   * @param Var The variable to set
   * @param Val The new value to store in the variable
   */
  void set_variable(const PGSTD::string &Var, const PGSTD::string &Val);//[t61]

  /// Get currently applicable value of variable
  /** This function will try to consult the cache of variables set (both in the
   * transaction and in the connection) using the set_variable functions.  If it
   * is not found there, the database is queried.
   *
   * @warning Do not mix the set_variable with raw "SET" queries, and do not
   * try to set or get variables while a pipeline or table stream is active.
   *
   * @warning This function used to be declared as @c const but isn't anymore.
   */
  PGSTD::string get_variable(const PGSTD::string &);			//[t61]

#ifdef PQXX_DEPRECATED_HEADERS
  /**
   * @name 1.x API
   */
  //@{
  /// @deprecated Use commit() instead
  void Commit() { commit(); }
  /// @deprecated Use abort() instead
  void Abort() { abort(); }
  /// @deprecated Use exec() instead
  result Exec(const char Q[], const PGSTD::string &D=PGSTD::string())
	{ return exec(Q,D); }
  /// @deprecated Use exec() instead
  result Exec(const PGSTD::string &Q, const PGSTD::string &D=PGSTD::string())
    	{ return exec(Q,D); }
  /// @deprecated Use process_notice() instead
  void ProcessNotice(const char M[]) const { return process_notice(M); }
  /// @deprecated Use process_notice() instead
  void ProcessNotice(const PGSTD::string &M) const { return process_notice(M); }
  /// @deprecated Use name() instead
  PGSTD::string Name() const { return name(); }
  /// @deprecated Use conn() instead
  connection_base &Conn() const { return conn(); }
  /// @deprecated Use set_variable() instead
  void SetVariable(const PGSTD::string &Var, const PGSTD::string &Val)
    	{ set_variable(Var,Val); }
  //@}
#endif

protected:
  /// Create a transaction (to be called by implementation classes only)
  /** The optional name, if nonempty, must begin with a letter and may contain
   * letters and digits only.
   */
  explicit transaction_base(connection_base &,
		          const PGSTD::string &TName,
			  const PGSTD::string &CName);

  /// Begin transaction (to be called by implementing class)
  /** Will typically be called from implementing class' constructor.
   */
  void Begin();

  /// End transaction.  To be called by implementing class' destructor
  void End() throw ();

  /// To be implemented by derived implementation class: start transaction
  virtual void do_begin() =0;
  /// To be implemented by derived implementation class: perform query
  virtual result do_exec(const char Query[]) =0;
  /// To be implemented by derived implementation class: commit transaction
  virtual void do_commit() =0;
  /// To be implemented by derived implementation class: abort transaction
  virtual void do_abort() =0;

  // For use by implementing class:

  /// Execute query on connection directly
  /**
   * @param C Query or command to execute
   * @param Retries Number of times to retry the query if it fails.  Be
   * extremely careful with this option; if you retry in the middle of a
   * transaction, you may be setting up a new connection transparently and
   * executing the latter part of the transaction without a backend transaction
   * being active (and with the former part aborted).
   */
  result DirectExec(const char C[], int Retries=0);

  /// Forget about any reactivation-blocking resources we tried to allocate
  void reactivation_avoidance_clear() throw () { m_reactivation_avoidance = 0; }

private:
  /* A transaction goes through the following stages in its lifecycle:
   * <ul>
   * <li> nascent: the transaction hasn't actually begun yet.  If our connection
   *    fails at this stage, it may recover and the transaction can attempt to
   *    establish itself again.
   * <li> active: the transaction has begun.  Since no commit command has been
   *    issued, abortion is implicit if the connection fails now.
   * <li> aborted: an abort has been issued; the transaction is terminated and
   *    its changes to the database rolled back.  It will accept no further
   *    commands.
   * <li> committed: the transaction has completed successfully, meaning that a
   *    commit has been issued.  No further commands are accepted.
   * <li> in_doubt: the connection was lost at the exact wrong time, and there
   *    is no way of telling whether the transaction was committed or aborted.
   * </ul>
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


  void PQXX_PRIVATE CheckPendingError();

  template<typename T> bool parm_is_null(T *p) const throw () { return !p; }
  template<typename T> bool parm_is_null(T) const throw () { return false; }

  friend class Cursor;
  friend class cursor_base;
  int GetUniqueCursorNum() { return m_UniqueCursorNum++; }
  void MakeEmpty(result &R) const { m_Conn.MakeEmpty(R); }
  void reactivation_avoidance_inc() throw () { ++m_reactivation_avoidance; }
  void reactivation_avoidance_dec() throw () { --m_reactivation_avoidance; }

  friend class internal::transactionfocus;
  void PQXX_PRIVATE RegisterFocus(internal::transactionfocus *);
  void PQXX_PRIVATE UnregisterFocus(internal::transactionfocus *) throw ();
  void PQXX_PRIVATE RegisterPendingError(const PGSTD::string &) throw ();
  friend class tablereader;
  void PQXX_PRIVATE BeginCopyRead(const PGSTD::string &, const PGSTD::string &);
  bool ReadCopyLine(PGSTD::string &L) { return m_Conn.ReadCopyLine(L); }
  friend class tablewriter;
  void PQXX_PRIVATE BeginCopyWrite(const PGSTD::string &Table,
      	const PGSTD::string &Columns = PGSTD::string());
  void WriteCopyLine(const PGSTD::string &L) { m_Conn.WriteCopyLine(L); }
  void EndCopyWrite() { m_Conn.EndCopyWrite(); }

  friend class pipeline;
  void start_exec(const PGSTD::string &Q) { m_Conn.start_exec(Q); }
  internal::pq::PGresult *get_result() { return m_Conn.get_result(); }
  void consume_input() throw () { m_Conn.consume_input(); }
  bool is_busy() const throw () { return m_Conn.is_busy(); }

  connection_base &m_Conn;

  int m_UniqueCursorNum;
  internal::unique<internal::transactionfocus> m_Focus;
  Status m_Status;
  bool m_Registered;
  PGSTD::map<PGSTD::string, PGSTD::string> m_Vars;
  PGSTD::string m_PendingError;
  /// Resources allocated in this transaction that make reactivation impossible
  /** This number may be negative!
   */
  int m_reactivation_avoidance;

  /// Not allowed
  transaction_base();
  /// Not allowed
  transaction_base(const transaction_base &);
  /// Not allowed
  transaction_base &operator=(const transaction_base &);
};

} // namespace pqxx


