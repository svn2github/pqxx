/*-------------------------------------------------------------------------
 *
 *   FILE
 *	pqxx/transaction.h
 *
 *   DESCRIPTION
 *      definition of the pqxx::transaction class.
 *   pqxx::transaction represents a database transaction
 *
 * Copyright (c) 2001-2003, Jeroen T. Vermeulen <jtv@xs4all.nl>
 *
 * See COPYING for copyright license.  If you did not receive a file called
 * COPYING with this source code, please notify the distributor of this mistake,
 * or contact the author.
 *
 *-------------------------------------------------------------------------
 */
#ifndef PQXX_TRANSACTION_H
#define PQXX_TRANSACTION_H

/* While you may choose to create your own transaction object to interface to 
 * the database backend, it is recommended that you wrap your transaction code 
 * into a transactor code instead and let the transaction be created for you.
 * See pqxx/transactor.h for more about transactor.
 *
 * If you should find that using a transactor makes your code less portable or 
 * too complex, go ahead, create your own transaction anyway.
 */

// Usage example: double all wages
//
// extern connection C;
// transaction<> T(C);
// try
// {
//   T.Exec("UPDATE employees SET wage=wage*2");
//   T.Commit();	// NOTE: do this inside try block
// } 
// catch (const exception &e)
// {
//   cerr << e.what() << endl;
//   T.Abort();		// Usually not needed; same happens when T's life ends.
// }

#include "pqxx/dbtransaction.h"

/* Methods tested in eg. self-test program test1 are marked with "//[t1]"
 */


// TODO: Any user-friendliness we can add to invoking stored procedures?

namespace pqxx
{

class PQXX_LIBEXPORT basic_transaction : public dbtransaction
{
protected:
  explicit basic_transaction(connection_base &C, 
			     const PGSTD::string &IsolationLevel,
			     const PGSTD::string &TName) :		//[t1]
    dbtransaction(C, IsolationLevel, TName) {}

private:
  virtual void DoBegin();						//[t1]
  virtual result DoExec(const char[]);					//[t1]
  virtual void DoCommit();						//[t1]
  virtual void DoAbort();						//[t13]

};


/// Human-readable class name for use by unique
template<> inline PGSTD::string Classname(const basic_transaction *) 
{ 
  return "basic_transaction"; 
}


/// Standard back-end transaction, templatized on isolation level
/** This is the type you'll normally want to use to represent a transaction on
 * the database.
 */
template<isolation_level ISOLATIONLEVEL=read_committed>
class PQXX_LIBEXPORT transaction : public basic_transaction
{
public:
  typedef isolation_traits<ISOLATIONLEVEL> isolation_tag;

  /// Create a transaction.  The optional name, if given, must begin with a
  /// letter and may contain letters and digits only.
  explicit transaction(connection_base &C,
		       const PGSTD::string &TName=PGSTD::string()) :	//[t1]
    basic_transaction(C, isolation_tag::name(), TName) 
    	{ Begin(); }

  virtual ~transaction() { End(); }
};


/// Human-readable class names for use by unique template.
template<isolation_level ISOLATIONLEVEL> 
inline PGSTD::string Classname(const transaction<ISOLATIONLEVEL> *) 
{ 
  return "transaction<" + isolation_traits<ISOLATIONLEVEL>::name() + ">";
}


/// @deprecated Compatibility with the old Transaction class
typedef transaction<read_committed> Transaction;

}

#endif

