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

/* gui_support.c - general (GUI) support routines */

#include "config.h"

#include <gtk/gtk.h>

#include "main.h"
#include "gui_support.h"

/* The dialog box that is currently open */
GtkWidget	*current_dialog = NULL;

static void choice_clicked(GtkWidget *widget, gpointer number)
{
	int	*choice_return;

	choice_return = gtk_object_get_data(GTK_OBJECT(widget),
			"choice_return");

	if (choice_return)
		*choice_return = (int) number;
}

/* Open a modal dialog box showing a message.
 * The user can choose from a selection of buttons at the bottom.
 * Returns -1 if the window is destroyed, or the number of the button
 * if one is clicked (starting from zero).
 *
 * If a dialog is already open, returns -1 without waiting AND
 * brings the current dialog to the front.
 */
int get_choice(char *title,
	       char *message,
	       int number_of_buttons, ...)
{
	GtkWidget	*dialog;
	GtkWidget	*vbox, *action_area, *separator;
	GtkWidget	*text, *text_container;
	GtkWidget	*button = NULL;
	int		i, retval;
	va_list	ap;
	int		choice_return;

	if (current_dialog)
	{
		gtk_widget_hide(current_dialog);
		gtk_widget_show(current_dialog);
		return -1;
	}

	current_dialog = dialog = gtk_window_new(GTK_WINDOW_DIALOG);
	GTK_WIDGET_UNSET_FLAGS(dialog, GTK_CAN_FOCUS);
	gtk_window_set_modal(GTK_WINDOW(dialog), TRUE);
	gtk_window_set_title(GTK_WINDOW(dialog), title);
	gtk_window_set_position(GTK_WINDOW(dialog), GTK_WIN_POS_CENTER);
	gtk_container_set_border_width(GTK_CONTAINER(dialog), 2);

	vbox = gtk_vbox_new(FALSE, 0);
	gtk_container_add(GTK_CONTAINER(dialog), vbox);

	action_area = gtk_hbox_new(TRUE, 5);
	gtk_container_set_border_width(GTK_CONTAINER(action_area), 2);
	gtk_box_pack_end(GTK_BOX(vbox), action_area, FALSE, TRUE, 0);

	separator = gtk_hseparator_new ();
	gtk_box_pack_end(GTK_BOX(vbox), separator, FALSE, TRUE, 2);

	text = gtk_label_new(message);
	gtk_label_set_line_wrap(GTK_LABEL(text), TRUE);
	text_container = gtk_event_box_new();
	gtk_container_set_border_width(GTK_CONTAINER(text_container), 40);
	gtk_container_add(GTK_CONTAINER(text_container), text);

	gtk_box_pack_start(GTK_BOX(vbox),
			text_container,
			TRUE, TRUE, 0);

	va_start(ap, number_of_buttons);

	for (i = 0; i < number_of_buttons; i++)
	{
		button = gtk_button_new_with_label(va_arg(ap, char *));
		GTK_WIDGET_SET_FLAGS(button, GTK_CAN_DEFAULT);
		gtk_misc_set_padding(GTK_MISC(GTK_BIN(button)->child), 16, 2);
		gtk_object_set_data(GTK_OBJECT(button), "choice_return",
				&choice_return);
		gtk_box_pack_start(GTK_BOX(action_area), button, TRUE, TRUE, 0);
		gtk_signal_connect(GTK_OBJECT(button), "clicked",
					choice_clicked, (gpointer) i);
	}

	gtk_window_set_focus(GTK_WINDOW(dialog), button);
	gtk_window_set_default(GTK_WINDOW(dialog), button);
	gtk_container_set_focus_child(GTK_CONTAINER(action_area), button);

	va_end(ap);

	gtk_object_set_data(GTK_OBJECT(dialog), "choice_return",
				&choice_return);
	gtk_signal_connect(GTK_OBJECT(dialog), "destroy", choice_clicked,
			(gpointer) -1);

	choice_return = -2;

	gtk_widget_show_all(dialog);

	while (choice_return == -2)
		g_main_iteration(TRUE);

	retval = choice_return;

	if (retval != -1)
		gtk_widget_destroy(dialog);

	current_dialog = NULL;

	return retval;
}

/* Display a message in a window with PROJECT as title */
void report_error(char *message, ...)
{
        va_list args;
	gchar *s;

	g_return_if_fail(message != NULL);

	va_start(args, message);
	s = g_strdup_vprintf(message, args);
	va_end(args);

#ifdef GTK2
	{
		GtkWidget *dialog;
		dialog = gtk_message_dialog_new(NULL,
				GTK_DIALOG_MODAL,
				GTK_MESSAGE_ERROR,
				GTK_BUTTONS_OK,
				"%s", s);
		gtk_window_set_position(GTK_WINDOW(dialog), GTK_WIN_POS_CENTER);
		gtk_dialog_set_default_response(GTK_DIALOG(dialog),
						GTK_RESPONSE_OK);
		gtk_dialog_run(GTK_DIALOG(dialog));
		gtk_widget_destroy(dialog);
	}
#else
	get_choice(PROJECT, s, 1, _("OK"));
#endif
	g_free(s);
}
