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

static GList *int_settings = NULL;

static void xsettings_changed(guchar *unused)
{
	GList	*next;

	if (!manager)
	{
		g_warning("ROX-Session not managing XSettings, so not "
			  "affecting anything...");
		return;
	}

	for (next = int_settings; next; next = next->next)
	{
		gchar *name = (guchar *) next->data;
		int value;

		value = option_get_int(name);
		xsettings_manager_set_int(manager, name, value);
	}

	xsettings_manager_notify(manager);
}

static void register_xsetting_int(char *name, int value)
{
	int_settings = g_list_append(int_settings, name);
	option_add_int(name, value, xsettings_changed);
}

static void terminate_xsettings(void *data)
{
	g_warning("ROX-Session is no longer the XSETTINGS manager!");
}

void session_init(void)
{
	if (xsettings_manager_check_running(gdk_display,
					    DefaultScreen(gdk_display)))
		g_warning("An XSETTINGS manager is already running. "
				"Not taking control of XSETTINGS...");
	else
		manager = xsettings_manager_new(gdk_display,
						DefaultScreen(gdk_display),
					        terminate_xsettings, NULL);

	register_xsetting_int("Net/DoubleClickTime", 250);
	register_xsetting_int("Gtk/CanChangeAccels", 1);

	if (manager)
		xsettings_changed(NULL);
}

void show_main_window(void)
{
	GtkWidget *window, *align, *button, *label, *hbox, *image;
	
	window = gtk_message_dialog_new(NULL, GTK_DIALOG_MODAL,
			 GTK_MESSAGE_QUESTION, GTK_BUTTONS_NONE,
			 "\nReally logout?\n");

	gtk_window_set_title(GTK_WINDOW(window), PROJECT);

	gtk_window_set_position(GTK_WINDOW(window), GTK_WIN_POS_CENTER);

	gtk_dialog_add_buttons(GTK_DIALOG(window),
			GTK_STOCK_PREFERENCES, 1,
			GTK_STOCK_CANCEL, GTK_RESPONSE_DELETE_EVENT,
			NULL);

	button = gtk_button_new();
	label = gtk_label_new_with_mnemonic("_Logout");
	gtk_label_set_mnemonic_widget(GTK_LABEL(label), button);

	image = gtk_image_new_from_stock(GTK_STOCK_QUIT, GTK_ICON_SIZE_BUTTON);
	hbox = gtk_hbox_new(FALSE, 2);

	align = gtk_alignment_new(0.5, 0.5, 0.0, 0.0);

	gtk_box_pack_start(GTK_BOX(hbox), image, FALSE, FALSE, 0);
	gtk_box_pack_end(GTK_BOX(hbox), label, FALSE, FALSE, 0);

	gtk_container_add(GTK_CONTAINER(button), align);
	gtk_container_add(GTK_CONTAINER(align), hbox);
	gtk_widget_show_all(button);

	gtk_dialog_add_action_widget(GTK_DIALOG(window),
			button, GTK_RESPONSE_YES);

	switch (gtk_dialog_run(GTK_DIALOG(window)))
	{
		case GTK_RESPONSE_YES:
			gtk_main_quit();
			break;
		case 1:
			options_show();
			break;
		default:
			break;
	}

	gtk_widget_destroy(window);
}
