/*-------------------------------------------------------------------------
 *
 *   FILE
 *	pqxx/dbtransaction.hxx
 *
 *   DESCRIPTION
 *      definition of the pqxx::dbtransaction abstract base class.
 *   pqxx::dbransaction defines a real transaction on the database
 *   DO NOT INCLUDE THIS FILE DIRECTLY; include pqxx/dbtransaction instead.
 *
 * Copyright (c) 2004-2005, Jeroen T. Vermeulen <jtv@xs4all.nl>
 *
 * See COPYING for copyright license.  If you did not receive a file called
 * COPYING with this source code, please notify the distributor of this mistake,
 * or contact the author.
 *
 *-------------------------------------------------------------------------
 */
#include "pqxx/compiler-public.hxx"

#include "pqxx/transaction_base"

namespace pqxx
{

/// Abstract base class responsible for bracketing a backend transaction
/** 
 * @addtogroup transaction Transaction classes
 *
 * Use a dbtransaction-derived object such as "work" (transaction<>) to enclose
 * operations on a database in a single "unit of work."  This ensures that the
 * whole series of operations either succeeds as a whole or fails completely.
 * In no case will it leave half-finished work behind in the database.
 *
 * Once processing on a transaction has succeeded and any changes should be
 * allowed to become permanent in the database, call Commit().  If something
 * has gone wrong and the changes should be forgotten, call Abort() instead.
 * If you do neither, an implicit Abort() is executed at destruction time.
 *
 * It is an error to abort a transaction that has already been committed, or to
 * commit a transaction that has already been aborted.  Aborting an already
 * aborted transaction or committing an already committed one has been allowed
 * to make errors easier to deal with.  Repeated aborts or commits have no
 * effect after the first one.
 *
 * Database transactions are not suitable for guarding long-running processes.
 * If your transaction code becomes too long or too complex, please consider
 * ways to break it up into smaller ones.  There's no easy, general way to do
 * this since application-specific considerations become important at this
 * point.
 *
 * The actual operations for beginning and committing/aborting the backend
 * transaction are implemented by a derived class.  The implementing concrete
 * class must also call Begin() and End() from its constructors and destructors,
 * respectively, and implement DoExec().
 */
class PQXX_LIBEXPORT dbtransaction : public transaction_base
{
protected:
  dbtransaction(connection_base &, const PGSTD::string &IsolationString);
  explicit dbtransaction(connection_base &, bool direct=true);

  virtual ~dbtransaction();

  /// Start a transaction on the backend and set desired isolation level
  void start_backend_transaction();

protected:
  /// Sensible default implemented here: begin backend transaction
  virtual void do_begin();						//[t1]
  /// Sensible default implemented here: perform query
  virtual result do_exec(const char Query[]);
  /// To be implemented by derived class: commit backend transaction
  virtual void do_commit() =0;
  /// Sensible default implemented here: abort backend transaction
  /** Default implementation does two things:
   * <ol>
   * <li>Clears the "connection reactivation avoidance counter"</li>
   * <li>Executes a ROLLBACK statement</li>
   * </ol>
   */
  virtual void do_abort();						//[t13]

  static PGSTD::string fullname(const PGSTD::string &ttype,
      	const PGSTD::string &isolation);

private:
  /// Precomputed SQL command to run at start of this transaction
  PGSTD::string m_StartCmd;
};


} // namespace pqxx

