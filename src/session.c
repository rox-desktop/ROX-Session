/* vi: set cindent:
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

#include <errno.h>

#include <gdk/gdkx.h>
#include <gtk/gtk.h>

#include "session.h"
#include "gui_support.h"
#include "options.h"

/* See http://www.freedesktop.org/standards/xsettings/xsettings.html */
XSettingsManager *manager = NULL;

typedef struct _Setting Setting;

struct _Setting {
	gchar *name;
	gchar *default_value;
	Option option;
};

static Setting settings[] = {
	{"Net/DoubleClickTime", "250"},
	{"Gtk/CanChangeAccels", "1"},
};

#define N_SETTINGS (sizeof(settings) / sizeof(*settings))

static void xsettings_changed(void)
{
	int i;

	if (!manager)
		return;

	for (i = 0; i < N_SETTINGS; i++)
		xsettings_manager_set_int(manager,
				settings[i].name,
				settings[i].option.int_value);

	xsettings_manager_notify(manager);
}

static void terminate_xsettings(void *data)
{
	g_warning("ROX-Session is no longer the XSETTINGS manager!");
}

void session_init(void)
{
	int i;

	if (xsettings_manager_check_running(gdk_display,
					    DefaultScreen(gdk_display)))
		g_warning("An XSETTINGS manager is already running. "
				"Not taking control of XSETTINGS...");
	else
		manager = xsettings_manager_new(gdk_display,
						DefaultScreen(gdk_display),
					        terminate_xsettings, NULL);

	for (i = 0; i < N_SETTINGS; i++)
	{
		option_add_string(&settings[i].option,
				  settings[i].name,
				  settings[i].default_value);
	}

	option_add_notify(xsettings_changed);

	if (manager)
		xsettings_changed();
}

static void show_session_options(void)
{
	if (!manager)
		report_error("ROX-Session not managing XSettings, so changes "
				"will have no immediate effect...");
	options_show();
}

void show_main_window(void)
{
	GtkWidget *window, *button;
	
	window = gtk_message_dialog_new(NULL, GTK_DIALOG_MODAL,
			 GTK_MESSAGE_QUESTION, GTK_BUTTONS_NONE,
			 "\nReally logout?\n");

	gtk_window_set_title(GTK_WINDOW(window), PROJECT);

	gtk_window_set_position(GTK_WINDOW(window), GTK_WIN_POS_CENTER);

	gtk_dialog_add_buttons(GTK_DIALOG(window),
			GTK_STOCK_PREFERENCES, 1,
			GTK_STOCK_CANCEL, GTK_RESPONSE_DELETE_EVENT,
			NULL);

	button = button_new_mixed(GTK_STOCK_QUIT, "_Logout");
	gtk_widget_show(button);

	gtk_dialog_add_action_widget(GTK_DIALOG(window),
			button, GTK_RESPONSE_YES);

	switch (gtk_dialog_run(GTK_DIALOG(window)))
	{
		case GTK_RESPONSE_YES:
			gtk_main_quit();
			break;
		case 1:
			show_session_options();
			break;
		default:
			break;
	}

	gtk_widget_destroy(window);
}
