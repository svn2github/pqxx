/*-------------------------------------------------------------------------
 *
 *   FILE
 *	result.cxx
 *
 *   DESCRIPTION
 *      implementation of the pqxx::result class and support classes.
 *   pqxx::result represents the set of result tuples from a database query
 *
 * Copyright (c) 2001-2003, Jeroen T. Vermeulen <jtv@xs4all.nl>
 *
 * See COPYING for copyright license.  If you did not receive a file called
 * COPYING with this source code, please notify the distributor of this mistake,
 * or contact the author.
 *
 *-------------------------------------------------------------------------
 */
#include <new>
#include <stdexcept>

#include "pqxx/except.h"
#include "pqxx/result.h"


using namespace PGSTD;


pqxx::result &pqxx::result::operator=(const pqxx::result &Other)
{
  if (Other.m_Result != m_Result)
  {
    LoseRef();
    MakeRef(Other);
  }
  return *this;
}


pqxx::result &pqxx::result::operator=(PGresult *Other)
{
  if (Other != m_Result)
  {
    LoseRef();
    MakeRef(Other);
  }
  return *this;
}



const pqxx::result::tuple pqxx::result::at(pqxx::result::size_type i) const
{
  if ((i < 0) || (i >= size()))
    throw out_of_range("Tuple number out of range");

  return operator[](i);
}


void pqxx::result::CheckStatus(const string &Query) const
{
  if (!m_Result)
    throw runtime_error("No result");

  switch (PQresultStatus(m_Result))
  {
  case PGRES_EMPTY_QUERY: // The string sent to the backend was empty.
  case PGRES_COMMAND_OK: // Successful completion of a command returning no data
  case PGRES_TUPLES_OK: // The query successfully executed
    break;

  case PGRES_COPY_OUT: // Copy Out (from server) data transfer started
  case PGRES_COPY_IN: // Copy In (to server) data transfer started
    break;

  case PGRES_BAD_RESPONSE: // The server's response was not understood
  case PGRES_NONFATAL_ERROR: // TODO: Is this one really an error?
  case PGRES_FATAL_ERROR:
    throw sql_error(PQresultErrorMessage(m_Result), Query);

  default:
    throw logic_error("Internal libpqxx error: "
		      "pqxx::result: Unrecognized response code " +
		      ToString(int(PQresultStatus(m_Result))));
  }
}


void pqxx::result::MakeRef(PGresult *Other)
{
  if (Other)
  {
    try
    {
      m_Refcount = new int(1);
    }
    catch (const bad_alloc &)
    {
      PQclear(Other);
      throw;
    }
  }
  m_Result = Other;
}


void pqxx::result::MakeRef(const pqxx::result &Other)
{
  m_Result = Other.m_Result;
  m_Refcount = Other.m_Refcount;
  if (Other.m_Refcount)
    ++*m_Refcount;
}


void pqxx::result::LoseRef() throw ()
{
  if (m_Refcount)
  {
    --*m_Refcount;
    if (*m_Refcount <= 0)
    {
      delete m_Refcount;
      m_Refcount = 0;
    }
  }

  if (!m_Refcount && m_Result)
  {
    PQclear(m_Result);
    m_Result = 0;
  }
}



pqxx::result::size_type pqxx::result::AffectedRows() const
{
  const char *const RowsStr = PQcmdTuples(m_Result);
  return RowsStr[0] ? atoi(RowsStr) : 0;
}


const char *pqxx::result::GetValue(pqxx::result::size_type Row, 
		                 pqxx::result::tuple::size_type Col) const
{
  return PQgetvalue(m_Result, Row, Col);
}


bool pqxx::result::GetIsNull(pqxx::result::size_type Row,
		           pqxx::result::tuple::size_type Col) const
{
  return PQgetisnull(m_Result, Row, Col) != 0;
}

pqxx::result::field::size_type 
pqxx::result::GetLength(pqxx::result::size_type Row,
                        pqxx::result::tuple::size_type Col) const
{
  return PQgetlength(m_Result, Row, Col);
}



// tuple

pqxx::result::field pqxx::result::tuple::operator[](const char f[]) const
{
  return field(*this, m_Home->ColumnNumber(f));
}


pqxx::result::field pqxx::result::tuple::at(const char f[]) const
{
  const int fnum = m_Home->ColumnNumber(f);
  if (fnum == -1)
    throw invalid_argument(string("Unknown field '") + f + "'");

  return field(*this, fnum);
}


pqxx::result::field pqxx::result::tuple::at(pqxx::result::tuple::size_type i) const
{
  if ((i < 0) || (i >= size()))
    throw out_of_range("Invalid field number");

  return operator[](i);
}



// const_iterator

pqxx::result::const_iterator pqxx::result::const_iterator::operator++(int)
{
  const_iterator Old(*this);
  m_Index++;
  return Old;
}


pqxx::result::const_iterator pqxx::result::const_iterator::operator--(int)
{
  const_iterator Old(*this);
  m_Index--;
  return Old;
}


