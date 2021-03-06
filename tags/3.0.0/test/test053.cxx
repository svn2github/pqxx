#include <iostream>
#include <sstream>

#include "test_helpers.hxx"

using namespace PGSTD;
using namespace pqxx;


// Test program for libpqxx: import file to large object
namespace
{
const string Contents = "Large object test contents";

class ImportLargeObject : public transactor<>
{
public:
  explicit ImportLargeObject(largeobject &O, const string &File) :
    transactor<>("ImportLargeObject"),
    m_Object(O),
    m_File(File)
  {
  }

  void operator()(argument_type &T)
  {
    m_Object = largeobject(T, m_File);
    cout << "Imported '" << m_File << "' "
            "to large object #" << m_Object.id() << endl;
  }

private:
  largeobject &m_Object;
  string m_File;
};


class ReadLargeObject : public transactor<>
{
public:
  explicit ReadLargeObject(largeobject &O) :
    transactor<>("ReadLargeObject"),
    m_Object(O)
  {
  }

  void operator()(argument_type &T)
  {
    char Buf[200];
    largeobjectaccess O(T, m_Object, ios::in);
    const size_t len = O.read(Buf, sizeof(Buf)-1);
    PQXX_CHECK_EQUAL(
	string(Buf, len),
	Contents,
	"Large object contents were mangled.");
  }

private:
  largeobject m_Object;
};


class DeleteLargeObject : public transactor<>
{
public:
  explicit DeleteLargeObject(largeobject O) : m_Object(O) {}

  void operator()(argument_type &T)
  {
    m_Object.remove(T);
  }

private:
  largeobject m_Object;
};


void test_053(connection_base &C, transaction_base &orgT)
{
  orgT.abort();

  largeobject Obj;

  C.perform(ImportLargeObject(Obj, "pqxxlo.txt"));
  C.perform(ReadLargeObject(Obj));
  C.perform(DeleteLargeObject(Obj));
}
} // namespace

PQXX_REGISTER_TEST_T(test_053, nontransaction)
