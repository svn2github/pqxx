#include <cassert>
#include <iostream>

#include <pqxx/cachedresult.h>
#include <pqxx/connection.h>
#include <pqxx/transaction.h>
#include <pqxx/result.h>

using namespace PGSTD;
using namespace pqxx;


// Test program for libpqxx.  Compare behaviour of a cachedresult to a regular
// result.
//
// Usage: test040 [connect-string]
//
// Where connect-string is a set of connection options in Postgresql's
// PQconnectdb() format, eg. "dbname=template1" to select from a database
// called template1, or "host=foo.bar.net user=smith" to connect to a
// backend running on host foo.bar.net, logging in as user smith.
int main(int, char *argv[])
{
  try
  {
    // Set up a connection to the backend
    connection C(argv[1]);

    // Begin a transaction acting on our current connection
    transaction<serializable> T(C, "test40");

    const char Query[] = "SELECT * FROM events";

    // Perform a query on the database, storing result tuples in R
    result R( T.Exec(Query) );

    string LastReason;

    for (int BlockSize = 2; BlockSize <= R.size()+1; ++BlockSize)
    {
      cachedresult CR(T, Query, "cachedresult", BlockSize);
      const cachedresult::size_type CRS = CR.size();
      if (CRS != R.size())
	throw logic_error("BlockSize " + ToString(BlockSize) + ": "
	                  "Expected " + ToString(R.size()) + " rows, "
			  "got " + ToString(CRS));
      if (CR.size() != CRS)
	throw logic_error("BlockSize " + ToString(BlockSize) + ": "
	                  "Inconsistent size (" + ToString(CRS) + " vs. " +
			  ToString(CR.size()) + ")");

      // Compare contents for CR with R
      for (result::size_type i = 0; i < R.size(); ++i)
      {
	string A, B;
	R.at(i).at(0).to(A);
	CR.at(i).at(0).to(B);
	if (A != B)
	  throw logic_error("BlockSize " + ToString(BlockSize) + ", "
	                    "row " + ToString(i) + ": "
			    "Expected '" + A + "', "
			    "got '" + B + "'");
      }

      // CR was asked to compute its size explicitly.  With CR2, we let the
      // object find out its size by reading rows until they run out.
      cachedresult CR2(T, Query, "cachedresult2", BlockSize);
      CR2.at(CRS - 1);
      try 
      { 
	CR2.at(CRS); 
      }
      catch (const exception &e) 
      {
	if (e.what() != LastReason)
	{
	  cerr << "(Expected) " << e.what() << endl;
	  LastReason = e.what();
	}
      }
      if (CR2.size() != CRS)
	throw logic_error("BlockSize " + ToString(BlockSize) + ": "
	                  "Inconsistent discovered size (" + 
			  ToString(CR2.size()) + " vs. " + ToString(CRS) + ")");
    }
  }
  catch (const sql_error &e)
  {
    cerr << "SQL error: " << e.what() << endl
         << "Query was: '" << e.query() << "'" << endl;
    return 1;
  }
  catch (const exception &e)
  {
    // All exceptions thrown by libpqxx are derived from std::exception
    cerr << "Exception: " << e.what() << endl;
    return 2;
  }
  catch (...)
  {
    // This is really unexpected (see above)
    cerr << "Unhandled exception" << endl;
    return 100;
  }

  return 0;
}


