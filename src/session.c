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

static GtkWidget *window = NULL;

static void window_destroyed(GtkWidget *widget, GtkWidget **pointer_address)
{
	*pointer_address = NULL;
}

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
	GtkWidget *vbox, *action_area, *button, *label, *hbox;
	
	if (window)
		gtk_widget_destroy(window);

	window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
	gtk_window_set_title(GTK_WINDOW(window), PROJECT);
	gtk_signal_connect(GTK_OBJECT(window), "destroy",
			GTK_SIGNAL_FUNC(window_destroyed), &window);
	gtk_container_set_border_width(GTK_CONTAINER(window), 4);

	gtk_window_set_position(GTK_WINDOW(window), GTK_WIN_POS_CENTER);

	vbox = gtk_vbox_new(FALSE, 0);
	gtk_container_add(GTK_CONTAINER(window), vbox);

	label = gtk_label_new("Really logout?");
	gtk_misc_set_padding(GTK_MISC(label), 10, 40);
	gtk_box_pack_start(GTK_BOX(vbox), label, TRUE, TRUE, 0);

	gtk_box_pack_start(GTK_BOX(vbox),
			gtk_hseparator_new(), FALSE, TRUE, 4);

	hbox = gtk_hbox_new(FALSE, 40);
	gtk_box_pack_end(GTK_BOX(vbox), hbox, FALSE, TRUE, 0);
	
	
	action_area = gtk_hbutton_box_new();
	gtk_button_box_set_layout(GTK_BUTTON_BOX(action_area),
					GTK_BUTTONBOX_START);
	gtk_box_pack_start(GTK_BOX(hbox), action_area, FALSE, TRUE, 0);

	button = gtk_button_new_with_label("Settings...");
	GTK_WIDGET_SET_FLAGS(button, GTK_CAN_DEFAULT);
	gtk_box_pack_start(GTK_BOX(action_area), button, FALSE, TRUE, 0);
	gtk_signal_connect(GTK_OBJECT(button), "clicked",
				GTK_SIGNAL_FUNC(options_show), NULL);
	gtk_signal_connect_object(GTK_OBJECT(button), "clicked",
				GTK_SIGNAL_FUNC(gtk_widget_destroy),
				GTK_OBJECT(window));

	
	action_area = gtk_hbutton_box_new();
	gtk_button_box_set_layout(GTK_BUTTON_BOX(action_area),
					GTK_BUTTONBOX_END);
	gtk_hbutton_box_set_spacing_default(4);
	gtk_box_pack_end(GTK_BOX(hbox), action_area, FALSE, TRUE, 0);

	button = gtk_button_new_with_label("Cancel");
	GTK_WIDGET_SET_FLAGS(button, GTK_CAN_DEFAULT);
	gtk_box_pack_end(GTK_BOX(action_area), button, FALSE, TRUE, 0);
	gtk_signal_connect_object(GTK_OBJECT(button), "clicked",
				GTK_SIGNAL_FUNC(gtk_widget_destroy),
				GTK_OBJECT(window));

	button = gtk_button_new_with_label("Logout");
	GTK_WIDGET_SET_FLAGS(button, GTK_CAN_DEFAULT);
	gtk_box_pack_end(GTK_BOX(action_area), button, FALSE, TRUE, 0);
	gtk_signal_connect_object(GTK_OBJECT(button), "clicked",
					gtk_main_quit, NULL);
	gtk_widget_grab_default(button);

	gtk_widget_show_all(window);
}
