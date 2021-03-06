#include <test_helpers.hxx>

using namespace PGSTD;
using namespace pqxx;

namespace
{
void test_simultaneous_transactions(connection_base &c, transaction_base &t)
{
  t.abort();

  nontransaction n1(c);
  PQXX_CHECK_THROWS(
	nontransaction n2(c),
	logic_error,
	"Allowed to open simultaneous nontransactions.");
}
} // namespace

PQXX_REGISTER_TEST(test_simultaneous_transactions)
