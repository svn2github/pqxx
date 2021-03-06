/*-------------------------------------------------------------------------
 *
 *   FILE
 *	pqxx/tablestream.h
 *
 *   DESCRIPTION
 *      definition of the pqxx::tablestream class.
 *   pqxx::tablestream provides optimized batch access to a database table
 *
 * Copyright (c) 2001-2003, Jeroen T. Vermeulen <jtv@xs4all.nl>
 *
 * See COPYING for copyright license.  If you did not receive a file called
 * COPYING with this source code, please notify the distributor of this mistake,
 * or contact the author.
 *
 *-------------------------------------------------------------------------
 */
#ifndef PQXX_TABLESTREAM_H
#define PQXX_TABLESTREAM_H

#include <string>

#include "pqxx/compiler.h"

/* Methods tested in eg. self-test program test1 are marked with "//[t1]"
 */


namespace pqxx
{

// TODO: Non-blocking access to help hide network latencies

class transaction_base;


/// Base class for streaming data to/from database tables.
/** A Tablestream enables optimized batch read or write access to a database 
 * table using PostgreSQL's COPY TO STDOUT and COPY FROM STDIN commands,
 * respectively.  These capabilities are implemented by its subclasses 
 * tablereader and tablewriter.
 * A Tablestream exists in the context of a transaction, and no other streams
 * or queries may be applied to that transaction as long as the stream remains
 * open.
 */
class PQXX_LIBEXPORT tablestream
{
public:
  tablestream(transaction_base &Trans, 
	      const PGSTD::string &Name, 
	      const PGSTD::string &Null=PGSTD::string());			//[t6]
  virtual ~tablestream() =0;						//[t6]

  PGSTD::string Name() const { return m_Name; }				//[t10]

protected:
  transaction_base &Trans() const throw () { return m_Trans; }
  PGSTD::string NullStr() const { return m_Null; }

private:
  transaction_base &m_Trans;
  PGSTD::string m_Name;
  PGSTD::string m_Null;

  // Not allowed:
  tablestream();
  tablestream(const tablestream &);
  tablestream &operator=(const tablestream &);
};

/// @deprecated For compatibility with the old TableStream class
typedef tablestream TableStream;

}

#endif

