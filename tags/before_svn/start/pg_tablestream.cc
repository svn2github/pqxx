/*-------------------------------------------------------------------------
 *
 *   FILE
 *	pg_tablestream.cc
 *
 *   DESCRIPTION
 *      implementation of the Pg::TableStream class.
 *   Pg::TableStream provides optimized batch access to a database table
 *
 * Copyright (c) 2001, Jeroen T. Vermeulen <jtv@xs4all.nl>
 *
 *-------------------------------------------------------------------------
 */
#include "pg_tablestream.h"
#include "pg_transaction.h"

using namespace PGSTD;


Pg::TableStream::TableStream(Transaction &STrans, string SName, string Null) :
  m_Trans(STrans),
  m_Name(SName),
  m_Null(Null)
{
  STrans.RegisterStream(this);
}


Pg::TableStream::~TableStream()
{
  m_Trans.UnregisterStream(this);
  m_Trans.EndCopy();
}

