/* vi: set cindent:
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

/* wm.c - runs the window manager and keeps it running */

#include "config.h"

#include <stdio.h>
#include <errno.h>
#include <sys/types.h>
#include <unistd.h>

#include <gtk/gtk.h>

#include "main.h"
#include "wm.h"
#include "choices.h"

/* List of commands to start window managers.
 * If the first entry is blank or doesn't work then the user is asked
 * to choose from the list.
 * Loaded from <Choices>/ROX-Session/WindowMans.
 */
static GList	*wms = NULL;

static GtkWidget	*choice_box = NULL;

pid_t	wm_pid = -1;

static guchar *wm_command = NULL;

/* Static prototypes */
static void load_wm_list(void);
static void choose_wm(void);
static gint ok_clicked(GtkWidget *button, GtkEditable *entry);
static void run_wm(guchar *command);

/****************************************************************
 *			EXTERNAL INTERFACE			*
 ****************************************************************/

#define BUFFER_SIZE 256

void start_window_manager(void)
{
	guchar 	*path;

	path = choices_find_path_load("DefaultWM", "ROX-Session");

	if (path)
	{
		FILE   *file;
		guchar buffer[BUFFER_SIZE];

		file = fopen(path, "rb");
		g_free(path);
		
		if (fgets(buffer, BUFFER_SIZE, file))
		{
			g_strstrip(buffer);
			run_wm(buffer);
			return;
		}
	}

	choose_wm();
}

void wm_process_died(void)
{
	wm_pid = -1;

	choose_wm();
}

/****************************************************************
 *			INTERNAL FUNCTIONS			*
 ****************************************************************/

static void choose_wm(void)
{
	GtkWidget	*vbox, *combo, *hbox, *button;
	
	if (wms)
	{
		GList *next;

		for (next = wms; next; next = next->next)
			g_free(next->data);
		g_list_free(wms);
		wms = NULL;
	}
	
	load_wm_list();

	if (choice_box)
		gtk_widget_destroy(choice_box);

	choice_box = gtk_window_new(GTK_WINDOW_DIALOG);
	gtk_container_set_border_width(GTK_CONTAINER(choice_box), 10);
	gtk_window_set_position(GTK_WINDOW(choice_box), GTK_WIN_POS_CENTER);

	vbox = gtk_vbox_new(FALSE, 10);
	gtk_container_add(GTK_CONTAINER(choice_box), vbox);
	
	gtk_box_pack_start(GTK_BOX(vbox),
			gtk_label_new("Choose a window manager:"),
			TRUE, TRUE, 0);

	combo = gtk_combo_new();
	gtk_combo_disable_activate(GTK_COMBO(combo));
	gtk_box_pack_start(GTK_BOX(vbox), combo, FALSE, TRUE, 0);
	GTK_WIDGET_SET_FLAGS(combo, GTK_CAN_FOCUS);
	gtk_signal_connect(GTK_OBJECT(GTK_COMBO(combo)->entry), "activate",
			GTK_SIGNAL_FUNC(ok_clicked), GTK_COMBO(combo)->entry);

	hbox = gtk_hbox_new(TRUE, 10);
	gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, TRUE, 0);

	button = gtk_button_new_with_label("OK");
	GTK_WIDGET_SET_FLAGS(button, GTK_CAN_DEFAULT);
	gtk_box_pack_start(GTK_BOX(hbox), button, TRUE, TRUE, 0);
	gtk_window_set_default(GTK_WINDOW(choice_box), button);
	gtk_signal_connect(GTK_OBJECT(button), "clicked",
			GTK_SIGNAL_FUNC(ok_clicked), GTK_COMBO(combo)->entry);

	button = gtk_button_new_with_label("Cancel");
	GTK_WIDGET_SET_FLAGS(button, GTK_CAN_DEFAULT);
	gtk_box_pack_start(GTK_BOX(hbox), button, TRUE, TRUE, 0);
	gtk_signal_connect_object(GTK_OBJECT(button), "clicked",
			GTK_SIGNAL_FUNC(gtk_widget_destroy),
			GTK_OBJECT(choice_box));

	gtk_combo_set_popdown_strings(GTK_COMBO(combo), wms);

	if (wm_command)
		gtk_entry_set_text(GTK_ENTRY(GTK_COMBO(combo)->entry),
				wm_command);
	
	gtk_widget_show_all(choice_box);
}

static void load_wm_list(void)
{
	FILE	*file;
	guchar 	*path;
	guchar	buffer[BUFFER_SIZE];

	path = choices_find_path_load("WindowMans", "ROX-Session");
	if (!path)
		path = g_strconcat(app_dir, "/WindowMans", NULL);

	file = fopen(path, "rb");
	if (file)
		while (1)
		{
			if (!fgets(buffer, BUFFER_SIZE, file))
				break;

			g_strstrip(buffer);
			if (*buffer == '#' || *buffer == '\0')
				continue;
			wms = g_list_prepend(wms, g_strdup(buffer));
		}
	else
		g_warning("fopen(%s) failed: %s\n", path, g_strerror(errno));

	g_free(path);

	wms = g_list_reverse(wms);
}

static gint ok_clicked(GtkWidget *button, GtkEditable *entry)
{
	guchar	*command, *path;

	command = gtk_editable_get_chars(entry, 0, -1);

	path = choices_find_path_save("DefaultWM", "ROX-Session", TRUE);
	if (path)
	{
		FILE	*file;

		file = fopen(path, "wb");
		if (file)
		{
			int len;
			len = strlen(command);
			if (fwrite(command, 1, strlen(command), file) < len)
				g_warning("fwrite(%s) failed: %s\n",
						path, g_strerror(errno));
			if (fclose(file) == EOF)
				g_warning("fclose(%s) failed: %s\n",
						path, g_strerror(errno));
		}

		g_free(path);
	}

	gtk_widget_destroy(choice_box);
	choice_box = NULL;

	run_wm(command);

	g_free(command);

	return 1;
}

/* Run this command. When it dies, show the WM choices box again. */
static void run_wm(guchar *command)
{
	g_return_if_fail(wm_pid == -1);

	if (wm_command)
		g_free(wm_command);
	wm_command = g_strdup(command);

	wm_pid = fork();
	
	if (wm_pid == -1)
	{
		g_warning("fork() failed: %s\n", g_strerror(errno));
		choose_wm();
	}

	if (wm_pid == 0)
	{
		char *argv[2];

		argv[0] = command;
		argv[1] = NULL;

		execvp(command, argv);

		g_warning("execvp(%s) failed: %s\n", command,
						g_strerror(errno));
		_exit(1);
	}
}
