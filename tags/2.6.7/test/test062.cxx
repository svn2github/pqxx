#include <pqxx/compiler-internal.hxx>

#include <iostream>

#include <pqxx/binarystring>
#include <pqxx/connection>
#include <pqxx/transaction>
#include <pqxx/result>

using namespace PGSTD;
using namespace pqxx;

// Example program for libpqxx.  Test binarystring functionality.
//
// Usage: test062 [connect-string]
//
// Where connect-string is a set of connection options in Postgresql's
// PQconnectdb() format, eg. "dbname=template1" to select from a database
// called template1, or "host=foo.bar.net user=smith" to connect to a
// backend running on host foo.bar.net, logging in as user smith.

int main(int, char *argv[])
{
  try
  {
    const string TestStr =
      "Nasty\n\030Test\n\t String with \200\277 weird bytes "
      "\r\0 and Trailer\\\\\0";

    connection C(argv[1]);
    work T(C, "test62");

    T.exec("CREATE TEMP TABLE pqxxbin (binfield bytea)");
    const string Esc = T.esc_raw(TestStr);
    T.exec("INSERT INTO pqxxbin VALUES ('" + Esc + "')");

    result R = T.exec("SELECT * from pqxxbin");
    T.exec("DELETE FROM pqxxbin");

    binarystring B( R.at(0).at(0) );

    if (B.empty())
      throw logic_error("Binary string became empty in conversion");

    cout << "original: " << TestStr << endl
	 << "returned: " << B.c_ptr() << endl;

    if (B.size() != TestStr.size())
      throw logic_error("Binary string got changed from " + 
	  to_string(TestStr.size()) + " to " + to_string(B.size()) + " bytes");

    binarystring::const_iterator c;
    binarystring::size_type i;
    for (i=0, c=B.begin(); i<B.size(); ++i, ++c)
    {
      if (c == B.end())
	throw logic_error("Premature end to binary string at " + to_string(i));

      const char x = TestStr.at(i), y = B.at(i), z = B.data()[i];
      const unsigned char ux = x, uy = y, uz = z;
      const unsigned int uix = ux, uiy = uy, uiz = uz;

      if (x != y)
      {
	throw logic_error("Binary string byte " + to_string(i) + " "
	    "got changed from '" + to_string(x) + "' "
	    "(" + to_string(uix) + ") "
	    "to '" + to_string(y) + "' "
	    "(" + to_string(uiy) + ")");
      }

      if (y != z)
      {
	throw logic_error("Inconsistent byte at offset " + to_string(i) + ": "
	    "operator[] says '" + to_string(y) + "' (" + to_string(uiy) + "), "
	    "data() says '" + to_string(z) + "' "
	    "(" + to_string(uiz) + ")");
      }
    }
    if (B.at(0) != B.front())
      throw logic_error("Something wrong with binarystring::front()");
    if (c != B.end())
      throw logic_error("end() of binary string not reached");
    --c;
    if (*c != B.back())
      throw logic_error("Something wrong with binarystring::back()");

#ifdef PQXX_HAVE_REVERSE_ITERATOR
    binarystring::const_reverse_iterator r;
    for (i=B.length(), r=B.rbegin(); i>0; --i, ++r)
    {
      if (r == B.rend())
	throw logic_error("Premature rend to binary string at " + to_string(i));

      if (char(B[i-1]) != char(TestStr.at(i-1)))
	throw logic_error("Reverse iterator differs at " + to_string(i));
    }
    if (r != B.rend())
      throw logic_error("rend() of binary string not reached");
#endif

    if (B.str() != TestStr)
      throw logic_error("Binary string got mangled: '" + B.str() + "'");

    const string TestStr2("(More conventional text)");
    T.exec("INSERT INTO pqxxbin VALUES ('" + TestStr2 + "')");
    R = T.exec("SELECT * FROM pqxxbin");
    binarystring B2(R.front().front());

    if (B2 == B)
      throw logic_error("Two different binarystrings say they're equal!");
    if (!(B2 != B))
      throw logic_error("Problem with binarystring::operator!=");

    binarystring B1c(B), B2c(B2);
    if (B1c != B)
      throw logic_error("Copied binarystring differs from original");
    if (!(B2c == B2))
      throw logic_error("Copied binarystring not equal to original");

    B1c.swap(B2c);

    if (B2c == B1c)
      throw logic_error("Swapped binarystrings say they're identical!");
    if (B2c != B)
      throw logic_error("Problem with binarystring::swap()");
    if (!(B1c == B2))
      throw logic_error("Problem with one of two swapped binarystrings");
  }
  catch (const sql_error &e)
  {
    // If we're interested in the text of a failed query, we can write separate
    // exception handling code for this type of exception
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

