#include <cmath>
#include <iostream>
#include <sstream>

#include <pqxx/pqxx>

using namespace PGSTD;
using namespace pqxx;


// Streams test program for libpqxx.  Insert a result field into various
// types of streams.
//
// Usage: test046 [connect-string]
//
// Where connect-string is a set of connection options in Postgresql's
// PQconnectdb() format, eg. "dbname=template1" to select from a database
// called template1, or "host=foo.bar.net user=smith" to connect to a
// backend running on host foo.bar.net, logging in as user smith.
int main(int, char *argv[])
{
  try
  {
    connection C(argv[1]);
    work T(C, "test46");
    result R( T.exec("SELECT count(*) FROM pg_tables") );

    cout << "Count was " << R.at(0).at(0) << endl;

    // Read the value into a stringstream
    stringstream I;
    I << R[0][0];

    // Now convert the stringstream into a numeric type
    long L, L2;
    I >> L;
    cout << "As a long, it's " << L << endl;

    R[0][0].to(L2);
    if (L != L2)
      throw logic_error("Different conversion methods gave different results!");

    float F, F2;
    stringstream I2;
    I2 << R[0][0];
    I2 >> F;
    cout << "As a float, it's " << F << endl;
    R[0][0].to(F2);
    if (fabs(F2-F) > 0.01)
      throw logic_error("Inconsistent floating-point result: " + to_string(F2));

    float F3;
    from_string(R[0][0].c_str(), F3);
    if (fabs(F3-F) > 0.01)
      throw logic_error("Inconsistent float result from from_string");

    double D;
    from_string(R[0][0].c_str(), D);
    if (fabs(D-F) > 0.01)
      throw logic_error("Inconsistent double result from from_string");

#if defined(PQXX_HAVE_LONG_DOUBLE)
    long double LD;
    from_string(R[0][0].c_str(), LD);
    if (fabs(LD-F) > 0.01)
      throw logic_error("Inconsistent long double result from from_string");
#endif

    string S, S2, S3;
    from_string(R[0][0].c_str(), S);
    from_string(string(R[0][0].c_str()), S2);
    from_string(R[0][0], S3);
    if (S != S2)
      throw runtime_error("from_string(const char[],std::string &) "
	  "is inconsistent with "
	  "from_string(const std::string &,std::string &)");
    if (S3 != S2)
      throw runtime_error("from_string(const result::field &,std::string &) "
	  "is inconsistent with "
	  "from_string(const std::string &,std::string &)");

    R = T.exec("SELECT 1=1");
    if (!R.at(0).at(0).as<bool>())
      throw logic_error("1=1 doesn't yield 'true'");
    R = T.exec("SELECT 2+2=5");
    if (R.at(0).at(0).as<bool>())
      throw logic_error("2+2=5 yields 'true'");
  }
  catch (const sql_error &e)
  {
    cerr << "SQL error: " << e.what() << endl
         << "Query was: '" << e.query() << "'" << endl;
    return 1;
  }
  catch (const exception &e)
  {
    cerr << "Exception: " << e.what() << endl;
    return 2;
  }

  return 0;
}


