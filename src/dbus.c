/*
 * $Id$
 *
 * ROX-Session, a very simple session manager
 * Copyright (C) 2002, Thomas Leonard, <tal197@users.sourceforge.net>.
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

#include "config.h"

#define ROX_CONTROL_NS "net.sf.rox.Session.Control"

#define ROX_SESSION_DBUS_SERVICE "net.sf.rox.Session"
#define ROX_SESSION_DBUS_SERVICE_TEST "net.sf.rox.SessionTest"

#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <signal.h>
#include <gtk/gtk.h>

#include "global.h"

#include "dbus.h"
#include "gui_support.h"
#include "session.h"
#include "log.h"
#include "main.h"

static gint dbus_pid = -1;
static DBusConnection *dbus_connection = NULL;

/* Read one line from fd and store in DBUS_SESSION_BUS_ADDRESS. */
static void read_address(gint fd)
{
	char buffer[256];
	GString *output;
	char *nl;

	output = g_string_new("DBUS_SESSION_BUS_ADDRESS=");

	while (1) {
		int got;
		got = read(fd, buffer, sizeof(buffer));
		if (got <= 0)
		{
			report_error("Error reading from D-BUS:\n%s",
				got == 0 ? "No address printed"
					 : g_strerror(errno));
			g_string_free(output, TRUE);
			break;
		}
		g_return_if_fail(got <= sizeof(buffer));
		g_string_append_len(output, buffer, got);

		nl = strchr(output->str, '\n');
		if (!nl)
			continue;
		*nl = '\0';
		putenv(output->str);
		/* (don't free string; it's part of the environment now) */
		g_string_free(output, FALSE);
		break;
	}
}

/* This is a nasty hack. The D-BUS daemon should notice stderr being
 * closed when we exit, and quit itself. But for now, we need to stop
 * multiple D-BUS processes from building up...
 */
static void kill_dbus(void)
{
	g_return_if_fail(dbus_pid != -1);
	kill(dbus_pid, SIGTERM);
	dbus_pid = -1;
}

static void lost_connection(void) {
	dbus_connection_unref(dbus_connection);
	dbus_connection = NULL;
	report_error(
		_("ROX-Session lost its connection to the D-BUS session bus.\n"
		"You should save your work and logout as soon as "
		"possible (many things won't work correctly without the bus, "
		"and this is the only way to restart it)."));
}

/* If we didn't generate any reply (error or OK), try to guess what the problem
 * was... (NULL => OOM)
 */
static DBusMessage *create_error_reply(DBusMessage *message)
{
	char *msg;
	DBusMessage *reply;

	if (dbus_message_get_type(message) != DBUS_MESSAGE_TYPE_METHOD_CALL)
		return dbus_message_new_error(message,
				ROX_SESSION_ERROR, "Message type is not message_call");

	msg = g_strdup_printf("Unknown method name '%s' on interface '%s'",
			dbus_message_get_member(message),
			dbus_message_get_interface(message));
	reply = dbus_message_new_error(message, ROX_SESSION_ERROR, msg);
	g_free(msg);

	return reply;
}

static DBusHandlerResult message_handler(DBusConnection *connection,
					 DBusMessage *message, void *user_data)
{
	DBusMessage *(*handler)(DBusMessage *, DBusError *) = user_data;
	DBusMessage *reply = NULL;
	DBusError error;

	g_return_val_if_fail(handler != NULL, DBUS_HANDLER_RESULT_NOT_YET_HANDLED);

	dbus_error_init(&error);

	/* Invoke the object's handler */
	reply = handler(message, &error);

	/* If we got an error, turn it into an error message reply */
	if (dbus_error_is_set(&error))
	{
		if (reply)
		{
			dbus_message_unref(reply);
			reply = dbus_message_new_error(message,
					ROX_SESSION_ERROR,
					"Internal error: got a reply and an error!");
		}
		else
			reply = dbus_message_new_error(message,
					ROX_SESSION_ERROR, error.message);
		dbus_error_free(&error);
	}
	else if (!reply)
		reply = create_error_reply(message);

	if (!reply)
		g_warning("Out of memory");
	else if (!dbus_connection_send(connection, reply, NULL))
		g_warning("Out of memory");

	if (reply)
		dbus_message_unref(reply);

	return DBUS_HANDLER_RESULT_HANDLED;
}

static DBusMessage *session_handler(DBusMessage *message, DBusError *error)
{
	if (dbus_message_is_method_call(message, ROX_CONTROL_NS, "ShowOptions"))
	{
		if (!dbus_message_get_args(message, error, DBUS_TYPE_INVALID))
			return NULL;
		show_session_options();
		return dbus_message_new_method_return(message);
	} else if (dbus_message_is_method_call(message, ROX_CONTROL_NS,
						"LogoutWithoutConfirm"))
	{
		gtk_main_quit();
		return dbus_message_new_method_return(message);
	} else if (dbus_message_is_method_call(message, ROX_CONTROL_NS,
						"ShowMessages"))
	{
		show_message_log();
		return dbus_message_new_method_return(message);
	}

	return NULL;
}

/* When a method is invoked on 'path', call handler to generate a reply. */
gboolean register_object_path(const char **path, MessageHandler handler)
{
	DBusObjectPathVTable vtable = {
		NULL,
		message_handler,
	};

	if (!dbus_connection_register_object_path(dbus_connection,
			path, &vtable, handler))
	{
		g_warning("Out of memory");
		return FALSE;
	}

	return TRUE;
}

/* TRUE on success */
static gboolean connect_to_bus(void)
{
	GError *error = NULL;
	DBusError derror;
	DBusGProxy *local = NULL;
	const char *session_path[] = {"Session", NULL};

	dbus_error_init(&derror);

	dbus_connection = dbus_bus_get_with_g_main(DBUS_BUS_SESSION, &error);
	if (error)
		goto err;
	dbus_connection_set_exit_on_disconnect(dbus_connection, FALSE);

	if (dbus_bus_service_exists(dbus_connection,
				test_mode ? ROX_SESSION_DBUS_SERVICE_TEST
					  : ROX_SESSION_DBUS_SERVICE,
				&derror))
	{
		report_error(_("ROX-Session is already running. Can't manage "
				"your session twice!"));
		exit(EXIT_FAILURE);
	}

	if (error)
		goto err;
	
	dbus_bus_acquire_service(dbus_connection,
			ROX_SESSION_DBUS_SERVICE,
			0,
			&derror);

	if (dbus_error_is_set(&derror))
		goto err;

	register_object_path(session_path, session_handler);

	/* Get notified when the bus dies */
	local = dbus_gproxy_new_for_peer(dbus_connection,
			DBUS_PATH_ORG_FREEDESKTOP_LOCAL,
			DBUS_INTERFACE_ORG_FREEDESKTOP_LOCAL);
	g_return_val_if_fail(local != NULL, FALSE);
	g_signal_connect(G_OBJECT(local), "destroy",
			G_CALLBACK(lost_connection), NULL);

	return TRUE;
err:
	report_error("Error connecting to D-BUS session bus:\n%s\n",
			error ? error->message : derror.message);
	if (error)
		g_error_free(error);
	else if (dbus_error_is_set(&derror))
		dbus_error_free(&derror);

	if (dbus_connection)
	{
		dbus_connection_disconnect(dbus_connection);
		dbus_connection_unref(dbus_connection);
		dbus_connection = NULL;
	}
	return FALSE;
}

/* Returns once the D-BUS session daemon is running and we have a connection
 * to it. Sets the environment variable. Reports error if ROX-Session is
 * already running.
 */
void dbus_init(void)
{
	GError *error = NULL;
	gint stdout_pipe = -1;
	gchar *argv[] = {"dbus-daemon-1", "--session", "--print-address", NULL};

	if (getenv("DBUS_SESSION_BUS_ADDRESS") != NULL)
	{
		if (connect_to_bus())
			return;
		/* It's actually fatal, since D-BUS seems to cache the
		 * bus address. Try again anyway in case it gets fixed...
		 */
	}

	g_spawn_async_with_pipes(NULL, argv, NULL,
			G_SPAWN_SEARCH_PATH,
			NULL, NULL,	/* Child setup */
			&dbus_pid,
			NULL, &stdout_pipe, NULL,
			&error);

	if (error)
	{
		report_error(_("%s\n\n"
				"You can get D-BUS from freedesktop.org.\n"
				"ROX-Session will not work correctly "
				"without it!"),
				error->message);
		
		g_error_free(error);
		return;
	}

	g_return_if_fail(stdout_pipe != -1);

	atexit(kill_dbus);

	read_address(stdout_pipe);

	close(stdout_pipe);

	connect_to_bus();
}
