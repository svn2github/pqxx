#include <cassert>
#include <iostream>

#include <pqxx/pqxx>

using namespace PGSTD;
using namespace pqxx;


// Test program for libpqxx.  Attempt to perform nested queries.
//
// Usage: test088
int main()
{
  try
  {
    connection C;
    if (!C.supports(connection_base::cap_nested_transactions))
    {
      cout << "Backend version does not support nested transactions. "
	"Skipping test." <<endl;
      return 0;
    }

    // Trivial test: create subtransactions, and commit/abort
    work T0(C, "T0");
    cout << T0.exec("SELECT 'T0 starts'")[0][0].c_str() << endl;
    try
    {
      subtransaction T0a(T0, "T0a");
      T0a.commit();
    }
    catch (const feature_not_supported &e)
    {
      cerr << e.what() << endl;
      return 0;
    }
    subtransaction T0b(T0, "T0b");
    T0b.abort();
    cout << T0.exec("SELECT 'T0 ends'")[0][0].c_str() << endl;
    T0.commit();

    // Basic functionality: perform query in subtransaction; abort, continue
    work T1(C, "T1");
    cout << T1.exec("SELECT 'T1 starts'")[0][0].c_str() << endl;
    subtransaction T1a(T1, "T1a");
      cout << T1a.exec("SELECT '  a'")[0][0].c_str() << endl;
      T1a.commit();
    subtransaction T1b(T1, "T1b");
      cout << T1b.exec("SELECT '  b'")[0][0].c_str() << endl;
      T1b.abort();
    subtransaction T1c(T1, "T1c");
      cout << T1c.exec("SELECT '  c'")[0][0].c_str() << endl;
      T1c.commit();
    cout << T1.exec("SELECT 'T1 ends'")[0][0].c_str() << endl;
    T1.commit();

    // Commit/rollback functionality
    work T2(C, "T2");
    const string Table = "test088";
    T2.exec("CREATE TEMP TABLE " + Table + "(no INTEGER, text VARCHAR)");

    T2.exec("INSERT INTO " + Table + " VALUES(1,'T2')");

    subtransaction T2a(T2, "T2a");
      T2a.exec("INSERT INTO "+Table+" VALUES(2,'T2a')");
      T2a.commit();
    subtransaction T2b(T2, "T2b");
      T2b.exec("INSERT INTO "+Table+" VALUES(3,'T2b')");
      T2b.abort();
    subtransaction T2c(T2, "T2c");
      T2c.exec("INSERT INTO "+Table+" VALUES(4,'T2c')");
      T2c.commit();
    const result R = T2.exec("SELECT * FROM " + Table + " ORDER BY no");
    for (result::const_iterator i=R.begin(); i!=R.end(); ++i)
      cout << '\t' << i[0].c_str() << '\t' << i[1].c_str() << endl;
    if (R.size() != 3)
      throw logic_error("Expected 3 results, got " + to_string(R.size()));
    int expected[3] = { 1, 2, 4 };
    for (result::size_type n=0; n<R.size(); ++n)
      if (R[n][0].as<int>() != expected[n])
	throw logic_error("Expected row no. " + to_string(expected[n]) + ", "
	    "got " + R[n][0].c_str());
  }
  catch (const sql_error &e)
  {
    cerr << "SQL error: " << e.what() << endl
         << "Query was: " << e.query() << endl;
    return 1;
  }
  catch (const exception &e)
  {
    cerr << "Exception: " << e.what() << endl;
    return 2;
  }
  catch (...)
  {
    cerr << "Unhandled exception" << endl;
    return 100;
  }

  return 0;
}


