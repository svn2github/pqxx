#include <iostream>
#include <stdexcept>

#include <pqxx/connection>
#include <pqxx/cursor>
#include <pqxx/transaction>
#include <pqxx/transactor>
#include <pqxx/result>

#include "test_helpers.hxx"

using namespace PGSTD;
using namespace pqxx;


// Test program for libpqxx.  Simulate "in-doubt" transaction failure.
namespace
{
// Transaction class that simulates connection failure during commit.
class basic_flakytransaction : public dbtransaction
{
public:
  bool simulate_failure;

protected:
  basic_flakytransaction(connection_base &C, const string &I) :
    namedclass("flakytransaction"),
    dbtransaction(C, I),
    simulate_failure(false)
	{}

private:
  virtual void do_commit()
  {
    if (simulate_failure) conn().simulate_failure();

    try
    {
      DirectExec("COMMIT");
    }
    catch (const exception &e)
    {
      if (conn().is_open())
      {
        if (simulate_failure)
	  throw logic_error("Connection did not simulate failure");
	cerr << "Unexpected exception (connection still open)" << endl;
	throw;
      }

      process_notice(e.what() + string("\n"));

      const string Msg =
	(simulate_failure ?
		"Simulating commit failure" :
		"UNEXPECTED COMMIT FAILURE");

      process_notice(Msg + "\n");
      throw in_doubt_error(Msg);
    }
  }
};


template<isolation_level ISOLATIONLEVEL=read_committed>
class flakytransaction : public basic_flakytransaction
{
public:
  typedef isolation_traits<ISOLATIONLEVEL> isolation_tag;

  explicit flakytransaction(connection_base &C, const string &TName) :
    namedclass(fullname("transaction",isolation_tag::name()), TName),
    basic_flakytransaction(C, isolation_tag::name())
	{ Begin(); }

  explicit flakytransaction(connection_base &C) :
    namedclass(fullname("transaction",isolation_tag::name())),
    basic_flakytransaction(C, isolation_tag::name())
	{ Begin(); }

  virtual ~flakytransaction() throw () { End(); }
};


// A transactor build to fail, at least for the first m_failcount commits
class FlakyTransactor : public transactor<flakytransaction<> >
{
  int m_failcount;
public:
  explicit FlakyTransactor(int failcount=0) : 
    transactor<flakytransaction<> >("FlakyTransactor"),
    m_failcount(failcount)
	{}

  void operator()(argument_type &T)
  {
    T.simulate_failure = (m_failcount > 0);

    T.exec("SELECT count(*) FROM pg_tables");
  }

  void on_doubt() throw ()
  {
    try
    {
      if (m_failcount > 0)
      {
        --m_failcount;
	cout << "(Expected) Transactor outcome in doubt" << endl;
      }
      else
      {
        cerr << "Transactor outcome in doubt!" << endl;
      }
    }
    catch (const exception &)
    {
    }
  }
};


void simulate(connection_base &C, int failures, int attempts)
{
  bool failed = true;
  try
  {
    C.perform(FlakyTransactor(failures), attempts);
    failed = false;
  }
  catch (const in_doubt_error &e)
  {
    if (!failures) throw;
    cout << "(Expected) " << e.what() << endl;
  }
  if (failures && !failed)
    throw logic_error("Simulated failure did not lead to in-doubt error");
}


void test_094(connection_base &C, transaction_base &orgT)
{
  orgT.abort();

  // Run without simulating failure
  cout << "Playing transactor without simulating failure..." << endl;
  simulate(C, 0, 1);

  // Simulate one failure, but succeed on retry
  cout << "Playing transactor with simulated failure..." << endl;
  simulate(C, 1, 2);
}
} // namespace

PQXX_REGISTER_TEST_T(test_094, nontransaction)
