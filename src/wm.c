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

/* wm.c - runs the window manager and keeps it running */

#include "config.h"

#include <stdio.h>
#include <errno.h>
#include <sys/types.h>
#include <unistd.h>
#include <signal.h>
#include <string.h>

#include <gtk/gtk.h>

#include "global.h"

#include "main.h"
#include "session.h"
#include "wm.h"
#include "choices.h"
#include "options.h"
#include "gui_support.h"

pid_t	wm_pid = -1;

static char *window_manager = NULL;	/* (use get/set_window_manager to access) */
static gboolean auto_restart_wm = FALSE;	/* TRUE while waiting for requested quit */

/* Static prototypes */
static void run_wm(void);
static void choose_wm(const char *message);
void set_window_manager(const char *command);
static const char *get_window_manager(void);
static gboolean available_in_path(const char *file);

/****************************************************************
 *			EXTERNAL INTERFACE			*
 ****************************************************************/

#define BUFFER_SIZE 256

/* Must have called settings_init first. */
void start_window_manager(void)
{
	if (!test_mode)
		run_wm();
}

void wm_process_died(void)
{
	wm_pid = -1;

	if (auto_restart_wm)
	{
		auto_restart_wm = FALSE;
		run_wm();
	}
	else
		choose_wm(_("Your window manager has crashed (or quit). "
				"Please restart it, or choose another window manager."));
}

/* Used by settings.c */
void set_window_manager(const char *command)
{
	GtkWidget *box;

	if (!*command)
		command = NULL;
	g_free(window_manager);
	window_manager = g_strdup(command);

	if (wm_pid == -1)
		return;

	box = gtk_message_dialog_new(NULL, GTK_DIALOG_MODAL, GTK_MESSAGE_QUESTION,
			GTK_BUTTONS_NONE,
			_("Your default window manager is now '%s'.\n"
			"Would you like to quit your current window manager and start the new "
			"one right now?"), get_window_manager());
	gtk_window_set_position(GTK_WINDOW(box), GTK_WIN_POS_CENTER);

	gtk_dialog_add_button(GTK_DIALOG(box), GTK_STOCK_NO, GTK_RESPONSE_CANCEL);
	gtk_dialog_add_button(GTK_DIALOG(box), GTK_STOCK_EXECUTE, GTK_RESPONSE_OK);
	gtk_dialog_set_default_response(GTK_DIALOG(box), GTK_RESPONSE_OK);

	auto_restart_wm = TRUE;

	if (gtk_dialog_run(GTK_DIALOG(box)) == GTK_RESPONSE_OK)
		kill(wm_pid, SIGTERM);

	gtk_widget_destroy(box);
}

/****************************************************************
 *			INTERNAL FUNCTIONS			*
 ****************************************************************/

/* Report 'message' to the user, and ask them to choose and restart a new window
 * manager. Calls set_window_manager() and run_wm().
 */
static void choose_wm(const char *message)
{
	GtkWidget *box;
	GtkWidget *vbox, *entry;

	box = gtk_dialog_new_with_buttons("Choose window manager",
			NULL, GTK_DIALOG_MODAL | GTK_DIALOG_NO_SEPARATOR,
			GTK_STOCK_CLOSE, GTK_RESPONSE_CANCEL,
			GTK_STOCK_EXECUTE, GTK_RESPONSE_OK,
			NULL);
	gtk_window_set_position(GTK_WINDOW(box), GTK_WIN_POS_CENTER);

	vbox = GTK_DIALOG(box)->vbox;

	gtk_box_pack_start(GTK_BOX(vbox), gtk_label_new(message), TRUE, TRUE, 0);

	gtk_dialog_set_default_response(GTK_DIALOG(box), GTK_RESPONSE_OK);

	entry = gtk_entry_new();
	gtk_box_pack_start(GTK_BOX(vbox), entry, FALSE, TRUE, 0);
	gtk_entry_set_text(GTK_ENTRY(entry), get_window_manager());

	gtk_widget_show_all(vbox);

	if (gtk_dialog_run(GTK_DIALOG(box)) == GTK_RESPONSE_OK)
	{
		char *new;
		new = gtk_editable_get_chars(GTK_EDITABLE(entry), 0, -1);
		gtk_widget_destroy(box);
		settings_set_string("ROX/WindowManager", new);
		g_free(new);
		run_wm();
	}
	else
		gtk_widget_destroy(box);

}

/* Run this default WM. When it dies, show the WM choices box again.
 * Also runs the login scrip the first time.
 */
static void run_wm(void)
{
	GError	*error = NULL;
	gint	pid;
	char	**argv = NULL;
	const char *wm;

	if (wm_pid != -1)
	{
		report_error(_("The window manager which ROX-Session "
				"started for you is still running. Quit it "
				"before starting a new one."));
		return;
	}

	wm = get_window_manager();

	if (g_shell_parse_argv(wm, NULL, &argv, &error))
		g_spawn_async(NULL, argv, NULL,
			G_SPAWN_SEARCH_PATH | G_SPAWN_DO_NOT_REAP_CHILD |
			G_SPAWN_STDOUT_TO_DEV_NULL,
			NULL, NULL, &pid, &error);
	g_strfreev(argv);
	argv = NULL;
		
	if (error)
	{
		char *message;
		wm_pid = -1;

		message = g_strdup_printf(
			_("Failed to start window manager:\n%s\n"
			"Please choose a new window manager and try again."),
				error->message);

		g_error_free(error);

		choose_wm(message);
		g_free(message);
	}
	else
	{
		wm_pid = pid;

		run_login_script();
	}
}

/* Return the command to start the window manager.
 * If no command is set, select a suitable one.
 * NULL to prompt the user to choose.
 */
static const char *get_window_manager(void)
{
	const char *fallbacks[] = {"xfwm4", "sawfish", "sawmill", "enlightenment", "wmaker",
			    "icewm", "blackbox", "fluxbox", "metacity", "kwin", "kwm",
			    "fvwm2", "fvwm", "4Dwm", "twm"};

	if (window_manager && *window_manager)
		return window_manager;

	/* If we have Zero Install, then OroboROX is the default window
	 * manager. Otherwise, ask the user to choose on first login.
	 */
	if (access("/uri/0install/rox.sourceforge.net", F_OK) == 0)
		return "0run 'rox.sourceforge.net/apps/OroboROX 2004-04-23'";
	else
	{
		int i;

		for (i = 0; i < G_N_ELEMENTS(fallbacks); i++)
		{
			if (available_in_path(fallbacks[i]))
				return fallbacks[i];
		}

		return "twm";
	}
}

/* From glib. */
static gchar *my_strchrnul(const gchar *str, gchar c)
{
	gchar *p = (gchar*) str;
	while (*p && (*p != c))
		++p;

	return p;
}

/* Based on code from glib. */
static gboolean available_in_path(const char *file)
{
	const gchar *path, *p;
	gchar *name, *freeme;
	size_t len;
	size_t pathlen;
	gboolean found = FALSE;

	path = g_getenv("PATH");
	if (path == NULL)
		path = "/bin:/usr/bin:.";

	len = strlen(file) + 1;
	pathlen = strlen(path);
	freeme = name = g_malloc(pathlen + len + 1);

	/* Copy the file name at the top, including '\0'  */
	memcpy(name + pathlen + 1, file, len);
	name = name + pathlen;
	/* And add the slash before the filename  */
	*name = '/';

	p = path;
	do
	{
		char *startp;

		path = p;
		p = my_strchrnul(path, ':');

		if (p == path)
			/* Two adjacent colons, or a colon at the beginning or the end
			 * of `PATH' means to search the current directory.
			 */
			startp = name + 1;
		else
			startp = memcpy (name - (p - path), path, p - path);

		/* Try to execute this name.  If it works, execv will not return.  */
		if (access(startp, X_OK) == 0)
			found = TRUE;
	} while (!found && *p++ != '\0');

	g_free(freeme);

	return found;
}
