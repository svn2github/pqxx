/*-------------------------------------------------------------------------
 *
 *   FILE
 *	pqxx/transaction.hxx
 *
 *   DESCRIPTION
 *      definition of the pqxx::transaction class.
 *   pqxx::transaction represents a standard database transaction
 *   DO NOT INCLUDE THIS FILE DIRECTLY; include pqxx/transaction instead.
 *
 * Copyright (c) 2001-2005, Jeroen T. Vermeulen <jtv@xs4all.nl>
 *
 * See COPYING for copyright license.  If you did not receive a file called
 * COPYING with this source code, please notify the distributor of this mistake,
 * or contact the author.
 *
 *-------------------------------------------------------------------------
 */
#include "pqxx/compiler-public.hxx"


#include "pqxx/dbtransaction"



/* Methods tested in eg. self-test program test1 are marked with "//[t1]"
 */


namespace pqxx
{

/**
 * @addtogroup transaction Transaction classes
 */
//@{

class PQXX_LIBEXPORT basic_transaction : public dbtransaction
{
protected:
  basic_transaction(connection_base &C,
			     const PGSTD::string &IsolationLevel);	//[t1]

private:
  virtual void do_commit();						//[t1]
};


/// Standard back-end transaction, templatized on isolation level
/** This is the type you'll normally want to use to represent a transaction on
 * the database.
 *
 * While you may choose to create your own transaction object to interface to
 * the database backend, it is recommended that you wrap your transaction code
 * into a transactor code instead and let the transaction be created for you.
 * @see pqxx/transactor.hxx
 *
 * If you should find that using a transactor makes your code less portable or
 * too complex, go ahead, create your own transaction anyway.
 *
 * Usage example: double all wages
 *
 * @code
 * extern connection C;
 * work T(C);
 * try
 * {
 *   T.exec("UPDATE employees SET wage=wage*2");
 *   T.commit();	// NOTE: do this inside try block
 * }
 * catch (const exception &e)
 * {
 *   cerr << e.what() << endl;
 *   T.abort();		// Usually not needed; same happens when T's life ends.
 * }
 * @endcode
 */
template<isolation_level ISOLATIONLEVEL=read_committed>
class transaction : public basic_transaction
{
public:
  typedef isolation_traits<ISOLATIONLEVEL> isolation_tag;

  /// Create a transaction
  /**
   * @param C Connection for this transaction to operate on
   * @param TName Optional name for transaction; must begin with a letter and
   * may contain letters and digits only
   */
  explicit transaction(connection_base &C, const PGSTD::string &TName):	//[t1]
    namedclass(fullname("transaction",isolation_tag::name()), TName),
    basic_transaction(C, isolation_tag::name())
    	{ Begin(); }

  explicit transaction(connection_base &C) :				//[t1]
    namedclass(fullname("transaction",isolation_tag::name())),
    basic_transaction(C, isolation_tag::name())
    	{ Begin(); }

  virtual ~transaction() throw ()
  {
#ifdef PQXX_QUIET_DESTRUCTORS
    internal::disable_noticer Quiet(conn());
#endif
    End();
  }

protected:
  virtual const char *classname() const throw () { return "transaction"; }
};


/// Bog-standard, default transaction type
typedef transaction<> work;

//@}

}


