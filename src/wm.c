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

#include <gtk/gtk.h>

#include "main.h"
#include "session.h"
#include "wm.h"
#include "choices.h"
#include "options.h"
#include "gui_support.h"

pid_t	wm_pid = -1;

static Option o_default_wm;

/* Static prototypes */
static GList *load_wm_list(void);
static void run_wm(void);
static GList *start_wm_button(Option *option, xmlNode *node, guchar *label);
static GList *wm_combo(Option *option, xmlNode *node, guchar *label);
static void choose_wm(void);

/****************************************************************
 *			EXTERNAL INTERFACE			*
 ****************************************************************/

#define BUFFER_SIZE 256

void start_window_manager(void)
{
	option_register_widget("start-wm", start_wm_button);
	option_register_widget("wm-combo", wm_combo);
		
	option_add_string(&o_default_wm, "DefaultWM", "");

	if (!test_mode)
		run_wm();
}

void wm_process_died(void)
{
	wm_pid = -1;

	report_error(_("Your window manager has crashed (or quit). "
		       "Please restart it, or choose another window manager."),
			o_default_wm.value);
	choose_wm();
}

/****************************************************************
 *			INTERNAL FUNCTIONS			*
 ****************************************************************/

static void choose_wm(void)
{
	GtkWidget *window;
	
	window = options_show();

	if (window)
		gtk_signal_connect_object(GTK_OBJECT(window), "destroy",
				GTK_SIGNAL_FUNC(run_login_script), NULL);
}

static GList *load_wm_list(void)
{
	GList	*wms = NULL;
	guchar 	*path;
	GError  *error = NULL;
	gchar	*data;

	path = choices_find_path_load("WindowMans", "ROX-Session");
	if (!path)
		path = g_strconcat(app_dir, "/WindowMans", NULL);

	g_file_get_contents(path, &data, NULL, &error);

	if (data)
	{
		gchar **lines;
		int	i;

		lines = g_strsplit(data, "\n", 0);

		g_free(data);

		for (i = 0; lines[i]; i++)
		{
			gchar *line = lines[i];
			
			if (line[0] && line[0] != '#')
				wms = g_list_prepend(wms, g_strdup(line));
		}

		g_strfreev(lines);
	}
	else
	{
		report_error(_("Error loading window manager list '%s':\n%s"),
				path, error->message);
		g_error_free(error);
	}

	g_free(path);

	return g_list_reverse(wms);
}

/* Run this default WM. When it dies, show the WM choices box again.
 * Also runs the login scrip the first time.
 */
static void run_wm(void)
{
	GError	*error = NULL;
	gint	pid;
	char	*argv[2];

	if (wm_pid != -1)
	{
		report_error(_("The window manager which ROX-Session "
				"started for you is still running. Quit it "
				"before starting a new one."));
		return;
	}

	if (!*o_default_wm.value)
	{
		report_error(_("No window manager is currently selected.\n"
			       "Please choose a window manager now."));
		choose_wm();
		return;
	}

	argv[0] = o_default_wm.value;
	argv[1] = NULL;
	g_spawn_async(NULL, argv, NULL,
			G_SPAWN_SEARCH_PATH | G_SPAWN_DO_NOT_REAP_CHILD,
			NULL, NULL, &pid, &error);
		
	if (error)
	{
		wm_pid = -1;

		report_error(_("Failed to start window manager:\n%s\n"
			"Please choose a new window manager and try again."),
				error->message);

		g_error_free(error);

		choose_wm();
	}
	else
	{
		wm_pid = pid;

		run_login_script();
	}
}

static void update_combo(Option *option)
{
	gtk_entry_set_text(GTK_ENTRY(option->widget), option->value);
}

static guchar *read_combo(Option *option)
{
	return gtk_editable_get_chars(GTK_EDITABLE(option->widget), 0, -1);
}

static GList *wm_combo(Option *option, xmlNode *node, guchar *label)
{
	GtkWidget	*combo, *entry, *hbox;
	GList		*wms;

	g_return_val_if_fail(option != NULL, NULL);

	hbox = gtk_hbox_new(FALSE, 4);

	gtk_box_pack_start(GTK_BOX(hbox), gtk_label_new(_(label)),
				FALSE, TRUE, 0);

	combo = gtk_combo_new();
	gtk_box_pack_start(GTK_BOX(hbox), combo, TRUE, TRUE, 0);
	entry = GTK_COMBO(combo)->entry;
	gtk_combo_disable_activate(GTK_COMBO(combo));
	GTK_WIDGET_SET_FLAGS(entry, GTK_CAN_FOCUS);

	wms = load_wm_list();
	if (wms)
	{
		GList *next;
		
		gtk_combo_set_popdown_strings(GTK_COMBO(combo), wms);

		for (next = wms; next; next = next->next)
			g_free(next->data);
		g_list_free(wms);
	}

	gtk_entry_set_text(GTK_ENTRY(entry), option->value);
	gtk_widget_grab_focus(entry);

	option->update_widget = update_combo;
	option->read_widget = read_combo;
	option->widget = entry;

	gtk_signal_connect_object_after(GTK_OBJECT(entry), "changed",
			GTK_SIGNAL_FUNC(option_check_widget),
			(GtkObject *) option);

	return g_list_append(NULL, hbox);
}

static GList *start_wm_button(Option *option, xmlNode *node, guchar *label)
{
	GtkWidget *button, *align;

	g_return_val_if_fail(option == NULL, NULL);
	
	align = gtk_alignment_new(0.5, 0.5, 0, 0);
	button = button_new_mixed(GTK_STOCK_YES, _("Start window manager"));
	gtk_container_add(GTK_CONTAINER(align), button);
	gtk_signal_connect(GTK_OBJECT(button), "clicked",
			GTK_SIGNAL_FUNC(run_wm), NULL);

	return g_list_append(NULL, align);
}
