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

#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <signal.h>
#include <gtk/gtk.h>

#include "global.h"

#include "dbus.h"
#include "gui_support.h"

static gint dbus_pid = -1;

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

/* Returns once the D-BUS session daemon is running and we have a connection
 * to it. Sets the environment variable.
 */
void dbus_init(void)
{
	GError *error = NULL;
	gint stdout_pipe = -1;
	gchar *argv[] = {"dbus-daemon-1", "--session", "--print-address", NULL};

	if (getenv("DBUS_SESSION_BUS_ADDRESS") != NULL)
		return;

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
}
