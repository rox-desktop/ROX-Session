/*
 * $Id$
 *
 * ROX-Session, a very simple session manager
 * Copyright (C) 2000, Thomas Leonard, <tal197@users.sourceforge.net>.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the Free
 * Software Foundation; either version 2 of the License, or (at your option)
 * any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program; if not, write to the Free Software Foundation, Inc., 59 Temple
 * Place, Suite 330, Boston, MA  02111-1307  USA
 */

/* session.c - implements the X Session Management Protocol */

/* Lots of bits from GNOME's session manager */

#include "config.h"

#include <stdlib.h>
#include <time.h>
#include <sys/types.h>
#include <unistd.h>

#include "gui_support.h"
#include "ice.h"
#include "session.h"

struct _Client {
	SmsConn connection;
};

static Status register_client(SmsConn connection,
			      SmPointer data, char *previous_id);
static void interact_request(SmsConn connection,
			     SmPointer data, int dialog_type);
static void interact_done(SmsConn connection, SmPointer data, gboolean cancel);
static void save_yourself_request(SmsConn connection,
			SmPointer data, int save_type,
		        gboolean shutdown, int interact_style,
			gboolean fast, gboolean global);
static void save_yourself_p2_request(SmsConn connection, SmPointer data);
static void save_yourself_done(SmsConn connection,
			       SmPointer data, gboolean success);
static void close_connection(SmsConn connection, SmPointer data, int count,
			     char **reasons);
static void set_properties(SmsConn connection, SmPointer data,
			   int nprops, SmProp **props);
static void delete_properties(SmsConn connection, SmPointer data,
			      int nprops, char **prop_names);
static void get_properties(SmsConn connection, SmPointer data);

	
/* Static prototypes */
//static void client_clean_up (Client* client);

void session_init(void)
{
	initialize_ice();
}

/* This is run when a new client connects.  We register all our
   callbacks.  */
Status new_client(SmsConn connection, SmPointer data, unsigned long *maskp,
		    SmsCallbacks *callbacks, char **reasons)
{
	gpointer client = NULL;

	g_print("[ new_client ]\n");
#if 0
  Client *client;

  if (shutting_down && !starting_warner)
    {
      *reasons  = strdup (_("A session shutdown is in progress."));
      return 0;
    }

  client = (Client*)g_new0 (Client, 1);
  client->priority = DEFAULT_PRIORITY;
  client->connection = connection;
  client->attempts = 1;
  client->connect_time = time (NULL);
  client->warning = FALSE;

#endif
  ice_set_clean_up_handler (SmsGetIceConnection (connection),
			    (GDestroyNotify)SmsCleanUp, connection);

  *maskp = 0;

  *maskp |= SmsRegisterClientProcMask;
  callbacks->register_client.callback = register_client;
  callbacks->register_client.manager_data = (SmPointer) client;

  *maskp |= SmsInteractRequestProcMask;
  callbacks->interact_request.callback = interact_request;
  callbacks->interact_request.manager_data = (SmPointer) client;

  *maskp |= SmsInteractDoneProcMask;
  callbacks->interact_done.callback = interact_done;
  callbacks->interact_done.manager_data = (SmPointer) client;

  *maskp |= SmsSaveYourselfRequestProcMask;
  callbacks->save_yourself_request.callback = save_yourself_request;
  callbacks->save_yourself_request.manager_data = (SmPointer) client;

  *maskp |= SmsSaveYourselfP2RequestProcMask;
  callbacks->save_yourself_phase2_request.callback = save_yourself_p2_request;
  callbacks->save_yourself_phase2_request.manager_data = (SmPointer) client;

  *maskp |= SmsSaveYourselfDoneProcMask;
  callbacks->save_yourself_done.callback = save_yourself_done;
  callbacks->save_yourself_done.manager_data = (SmPointer) client;

  *maskp |= SmsCloseConnectionProcMask;
  callbacks->close_connection.callback = close_connection;
  callbacks->close_connection.manager_data = (SmPointer) client;

  *maskp |= SmsSetPropertiesProcMask;
  callbacks->set_properties.callback = set_properties;
  callbacks->set_properties.manager_data = (SmPointer) client;

  *maskp |= SmsDeletePropertiesProcMask;
  callbacks->delete_properties.callback = delete_properties;
  callbacks->delete_properties.manager_data = (SmPointer) client;

  *maskp |= SmsGetPropertiesProcMask;
  callbacks->get_properties.callback = get_properties;
  callbacks->get_properties.manager_data = (SmPointer) client;

  return 1;
}

/* previous_id is NULL if this is a new client. For returning clients,
 * this is the previous ID. We really don't care, because we don't store
 * anything between sessions anyway.
 */
static Status register_client(SmsConn connection,
			      SmPointer data, char *previous_id)
{
	char *id;

	g_print("[ register_client(%s) ]\n", previous_id);

	if (previous_id)
		id = previous_id;
	else
	{
		id = SmsGenerateClientID(connection);

		if (!id)
		{
			static long int sequence = 0;
			static char* address = NULL;

			if (!address)
			{
				g_warning("Host name lookup failure "
					  "on localhost.");

				srand(time(NULL) + (getpid() << 16));
				address = g_strdup_printf("0%.8x", rand());
			};

			/* The typecast there is for 64-bit machines */
			id = g_strdup_printf("1%s%.13ld%.10ld%.4ld", address,
					(long) time(NULL),
					(long) getpid(), sequence);
			sequence++;

			sequence %= 10000;
		}
	}

	SmsRegisterClientReply(connection, id);

	g_free(id);

	return 1;
}

static void interact_request(SmsConn connection,
			     SmPointer data, int dialog_type)
{
	g_print("[ interact_request ]\n");
}
	
static void interact_done(SmsConn connection, SmPointer data, gboolean cancel)
{
	g_print("[ interact_done ]\n");
}

static void save_yourself_request(SmsConn connection,
			SmPointer data, int save_type,
		        gboolean shutdown, int interact_style,
			gboolean fast, gboolean global)
{
	g_print("[ save_yourself_request ]\n");
}

static void save_yourself_p2_request(SmsConn connection, SmPointer data)
{
	g_print("[ save_yourself_p2_request ]\n");
}

static void save_yourself_done(SmsConn connection,
			       SmPointer data, gboolean success)
{
	g_print("[ save_yourself_done ]\n");
}

/* We call IceCloseConnection in response to SmcCloseConnection to prevent
 * any use of the connection until we exit IceProcessMessages.
 * The actual clean up occurs in client_clean_up when the IceConn closes. */
static void close_connection(SmsConn connection, SmPointer data, int count,
			     char **reasons)
{
	IceConn ice_conn = SmsGetIceConnection (connection);

	g_print("[ close_connection ]\n");

	if (count > 0)
	{
		GString	*msg;
		int	n;

		msg = g_string_new("Client closed connection to session "
				"manager, saying:\n");
		for (n = 0; n < count; n++)
		{
			g_string_append(msg, reasons[n]);
			g_string_append_c(msg, '\n');
		}
		
		delayed_error(PROJECT, msg->str);

		g_string_free(msg, TRUE);
	}

	SmFreeReasons(count, reasons);
	IceSetShutdownNegotiation(ice_conn, FALSE); /* XXX: What's this? */
	IceCloseConnection(ice_conn);
}

/* This extends the standard SmsSetPropertiesProc by interpreting attempts
 * to set properties with the name GsmCommand as commands. These
 * properties are not added to the clients property list and using them
 * results in a change to the semantics of the SmsGetPropertiesProc
 * (until a GsmSuspend command is received). */
static void set_properties(SmsConn connection, SmPointer data,
			   int nprops, SmProp **props)
{
	g_print("[ set_properties ]\n");
}

static void delete_properties(SmsConn connection, SmPointer data,
			      int nprops, char **prop_names)
{
	g_print("[ delete_properties ]\n");
}

/* While the GsmCommand protocol is active the properties that are
 * returned represent the return values from the commands. All commands
 * return themselves as the first property returned. Commands need not
 * return values in the order in which they were invoked. The standard 
 * SmsGetPropertiesProc behavior may be restored at any time by suspending
 * the protocol using a GsmSuspend command. The next GsmCommand will 
 * then resume the protocol as if nothing had happened. */
static void get_properties(SmsConn connection, SmPointer data)
{
	g_print("[ get_properties ]\n");
}
