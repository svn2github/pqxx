/*-------------------------------------------------------------------------
 *
 *   FILE
 *	pqxx/trigger.h
 *
 *   DESCRIPTION
 *      definition of the pqxx::trigger functor interface.
 *   pqxx::trigger describes a database trigger to wait on, and what it does
 *
 * Copyright (c) 2001-2003, Jeroen T. Vermeulen <jtv@xs4all.nl>
 *
 * See COPYING for copyright license.  If you did not receive a file called
 * COPYING with this source code, please notify the distributor of this mistake,
 * or contact the author.
 *
 *-------------------------------------------------------------------------
 */
#ifndef PQXX_TRIGGER_H
#define PQXX_TRIGGER_H

#include <string>

/* Methods tested in eg. self-test program test1 are marked with "//[t1]"
 */

namespace pqxx
{

/// "Observer" base class for trigger notifications.
/** To listen on a database trigger, derive your own class from trigger and
 * define its function call operator to perform whatever action you wish to
 * take when the given trigger arrives.  Then create an object of that class
 * and pass it to your connection.  DO NOT set triggers directly through SQL,
 * or they won't be restored when your connection fails--and you'll have no
 * way to notice.
 *
 * Trigger notifications never arrive inside a transaction.  Therefore, you
 * are free to open a transaction of your own inside your trigger's function
 * invocation operator.
 *
 * Notifications for your trigger may arrive anywhere within libpqxx code, but
 * be aware that POSTGRESQL DEFERS NOTIFICATIONS OCCURRING INSIDE TRANSACTIONS.
 * So if you're keeping a transaction open, don't expect any of your triggers
 * on the same connection to be notified.
 *
 * Multiple triggers on the same connection may have the same name.
 */
class PQXX_LIBEXPORT trigger : public PGSTD::unary_function<int, void>
{
  // TODO: Rename to trigger
public:
  /// Constructor.  Registers the trigger with connection C.
  /**
   * @param C the connection this trigger resides in.
   * @param N a name for the trigger.
   */
  trigger(connection_base &C, const PGSTD::string &N) : 		//[t4]
    m_Conn(C), m_Name(N) { m_Conn.AddTrigger(this); }

  virtual ~trigger() { m_Conn.RemoveTrigger(this); }			//[t4]

  PGSTD::string Name() const { return m_Name; }				//[t4]

  /// Overridable: action to invoke when trigger is notified.
  /**
   * @param be_pid the process ID of the database backend process that served
   * our connection when the trigger was notified.  The actual process ID behind
   * the connection may have changed by the time this method is called.
   */
  virtual void operator()(int be_pid) =0;				//[t4]

protected:
  connection_base &Conn() const throw () { return m_Conn; }		//[t23]

private:
  connection_base &m_Conn;
  PGSTD::string m_Name;
};

/// @deprecated For compatibility with the old Trigger class
typedef trigger Trigger;

}


#endif

