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

/* log.c - handles the log window which captures standard error output */

#include "config.h"

#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <fcntl.h>
#include <time.h>

#include <gtk/gtk.h>
#include <gdk/gdkx.h>

#include "global.h"

#include "wm.h"
#include "main.h"
#include "log.h"
#include "options.h"
#include "gui_support.h"

#define MARGIN 4

static Option o_time_shown;

static GtkWidget *log_window = NULL;	/* The popup message window */
static GtkWidget *label = NULL;	/* Contains the log messages */
static gint	 input_tag;

static int	real_stderr = -1;

/* Concat all the chunks together to get the current message... */
typedef struct _Chunk Chunk;
struct _Chunk {
	time_t time;
	gchar  *text;
};
static GList	*chunks = NULL;

/* This holds history of recent log messages, which can be displayed
 * in a normal window.
 */
#define MAX_BUFFER_SIZE 10000			/* In characters */
static GtkTextBuffer *buffer = NULL;
static GtkWindow *message_window = NULL;	/* The non-popup window */

/* Static prototypes */
static void got_log_data(gpointer data,
			 gint source,
			 GdkInputCondition condition);
static gint log_clicked(GtkWidget *text, GdkEventButton *bev, gpointer data);
static void log_msg(const gchar *text, gint len);
static GList *show_log(Option *option, xmlNode *node, guchar *label);

/****************************************************************
 *			EXTERNAL INTERFACE			*
 ****************************************************************/

void log_init(void)
{

	int fds[2];

	buffer = gtk_text_buffer_new(NULL);

	option_add_int(&o_time_shown, "log_time_shown", 5);

	option_register_widget("show-log", show_log);

	if (pipe(fds))
	{
		g_warning("pipe(): %s\n", g_strerror(errno));
		return;
	}

	/* Grab a copy of stderr before we replace it.
	 * We'll duplicate output here if it exists...
	 */
	real_stderr = dup(STDERR_FILENO);

	/* Dup the writeable FD to stderr */
	if (fds[1] != STDERR_FILENO)
	{
		if (dup2(fds[1], STDERR_FILENO) == -1)
		{
			g_warning("dup2(): %s\n", g_strerror(errno));
			return;
		}
		close(fds[1]);
	}

	/* Don't close on exec()... */
	if (fcntl(STDERR_FILENO, F_SETFD, FALSE))
		g_warning("fcntl(): %s\n", g_strerror(errno));

	/* But, do close the other end */
	if (fcntl(fds[0], F_SETFD, TRUE))
		g_warning("fcntl(): %s\n", g_strerror(errno));

	log_window = gtk_window_new(GTK_WINDOW_POPUP);
	gtk_window_set_resizable(GTK_WINDOW(log_window), FALSE);
	gtk_widget_set_name(log_window, "log_window");
	gtk_widget_realize(log_window);
	gdk_window_set_override_redirect(log_window->window, TRUE);
	gtk_widget_add_events(log_window, GDK_BUTTON_PRESS_MASK);
	gtk_signal_connect(GTK_OBJECT(log_window), "button_press_event",
			GTK_SIGNAL_FUNC(log_clicked), NULL);

	label = gtk_label_new(NULL);
	gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.0);
	gtk_widget_add_events(label, GDK_BUTTON_PRESS_MASK);
	gtk_container_add(GTK_CONTAINER(log_window), label);
	gtk_widget_show(label);

	input_tag = gdk_input_add(fds[0], GDK_INPUT_READ, got_log_data, NULL);
}

/****************************************************************
 *			INTERNAL FUNCTIONS			*
 ****************************************************************/

/* Chunks has changed. Update the label (or hide if nothing to show). */
static void show(void)
{
	static gboolean top;
	int	h, y;
	GString *text;
	GList	*next;
	int	last_non_space = -1;

	if (message_window || !chunks)
	{
		gtk_widget_hide(log_window);
		return;
	}

	h = gdk_screen_height();
	if (!GTK_WIDGET_VISIBLE(log_window))
	{
		gdk_window_get_pointer(GDK_ROOT_PARENT(), NULL, &y, NULL);

		top = 2 * y > h;
	}

	text = g_string_new(NULL);
	for (next = chunks; next; next = next->next)
	{
		Chunk   *chunk = (Chunk *) next->data;

		g_string_append(text, chunk->text);
	}

	{
		gboolean nl = TRUE;
		int i;

		for (i = 0; i < text->len; i++)
		{
			if (text->str[i] == '\n')
			{
				if (nl)
					g_string_erase(text, i, 1);
				nl = TRUE;
			}
			else
			{
				last_non_space = i;
				nl = FALSE;
			}
		}
	}

	g_string_truncate(text, last_non_space + 1);

	gtk_label_set_text(GTK_LABEL(label), text->str);

	g_string_free(text, TRUE);

	if (text->len)
	{
		GtkRequisition req;
		int	max_h;

		gtk_widget_size_request(label, &req);
		max_h = gdk_screen_height() / 3;
		req.height = MIN(req.height, max_h);

		if (top)
			y = MARGIN;
		else
			y = h - MARGIN - req.height;

		req.width = gdk_screen_width() - 2 * MARGIN;

		gtk_widget_set_uposition(log_window, MARGIN, y);
		gtk_widget_set_usize(log_window, req.width, req.height);
		gtk_widget_show(log_window);
		gdk_window_move(log_window->window, MARGIN, y);
		gdk_window_raise(log_window->window);
	}
	else
		gtk_widget_hide(log_window);
}

/* Remove old entries */
static gboolean prune(gpointer data)
{
	static  gint prune_timeout = 0;
	gboolean did_prune = FALSE;
	time_t	prune_time;

	prune_time = time(NULL) - o_time_shown.int_value;

	while (chunks)
	{
		Chunk *first = (Chunk *) chunks->data;

		if (first->time > prune_time)
			break;		/* Still want to show this */

		chunks = g_list_remove(chunks, first);
		g_free(first->text);
		g_free(first);

		did_prune = TRUE;
	}

	if (did_prune)
		show();

	if (chunks)
	{
		if (prune_timeout)
			gtk_timeout_remove(prune_timeout);
		prune_timeout = gtk_timeout_add(1000, prune, NULL);
	}

	return FALSE;
}

static void log_msg(const gchar *text, gint len)
{
	Chunk   *new;
	int	chars;
	GtkTextIter end;

	if (len < 0)
		len = strlen(text);

	/* Add to the long history buffer... */
	gtk_text_buffer_get_end_iter(buffer, &end);
	gtk_text_buffer_insert(buffer, &end, text, len);

	/* Remove stuff from the beginning if it gets too long... */
	chars = gtk_text_buffer_get_char_count(buffer);
	if (chars > MAX_BUFFER_SIZE)
	{
		GtkTextIter start;

		gtk_text_buffer_get_start_iter(buffer, &start);
		end = start;
		gtk_text_iter_forward_chars(&end, chars - MAX_BUFFER_SIZE);
		gtk_text_iter_forward_line(&end);
		gtk_text_buffer_delete(buffer, &start, &end);
	}


	/* And to the popup message area... */
	new = g_new(Chunk, 1);

	time(&new->time);
	new->text = g_strndup(text, len);
	chunks = g_list_append(chunks, new);

	prune(NULL);
	show();
}

static void write_stderr(guchar *buffer, int len)
{
	int	pos = 0, sent;

	while (pos < len)
	{
		sent = write(real_stderr, buffer + pos, len - pos);
		if (sent < 0)
		{
			log_msg(_("ROX-Session: write to stderr failed!\n"),
					-1);
			close(real_stderr);
			real_stderr = -1;
			return;
		}
		pos += sent;
	}
}

#define BUFFER_SIZE 256

static void got_log_data(gpointer data,
			 gint source,
			 GdkInputCondition condition)
{
	guchar	buffer[BUFFER_SIZE];
	int	got;
	int 	error = login_error;

	login_error = 0;

	got = read(source, buffer, BUFFER_SIZE);

	if (got <= 0)
	{
		gtk_input_remove(input_tag);
		log_msg(_("ROX-Session: read(stderr) failed!\n"), -1);
	}
	else
	{
		log_msg(buffer, got);

		if (real_stderr != -1)
			write_stderr(buffer, got);
	}

	if (error)
		login_failure(error);

	if (wm_pid == -2)
		wm_process_died();
}

static void show_message_log(void)
{
	GtkWidget *view, *hbox, *bar, *dialog, *frame;

	if (message_window)
	{
		gtk_window_present(message_window);
		return;
	}

	dialog = gtk_dialog_new_with_buttons(_("ROX-Session message log"),
					     NULL, GTK_DIALOG_NO_SEPARATOR,
					     GTK_STOCK_CLOSE, GTK_RESPONSE_YES,
					     NULL);
	message_window = GTK_WINDOW(dialog);
	g_signal_connect(dialog, "response",
			G_CALLBACK(gtk_widget_destroy), NULL);
	g_signal_connect(message_window, "destroy",
			G_CALLBACK(gtk_widget_destroyed), &message_window);

	hbox = gtk_hbox_new(FALSE, 0);
	gtk_container_add(GTK_CONTAINER(GTK_DIALOG(dialog)->vbox), hbox);

	frame = gtk_frame_new(NULL);
	gtk_frame_set_shadow_type(GTK_FRAME(frame), GTK_SHADOW_IN);
	gtk_box_pack_start(GTK_BOX(hbox), frame, TRUE, TRUE, 0);

	view = gtk_text_view_new_with_buffer(buffer);
	gtk_text_view_set_editable(GTK_TEXT_VIEW(view), FALSE);
	gtk_text_view_set_cursor_visible(GTK_TEXT_VIEW(view), FALSE);
	gtk_text_view_set_wrap_mode(GTK_TEXT_VIEW(view), GTK_WRAP_WORD);
	gtk_widget_set_size_request(view, 400, 100);
	gtk_container_add(GTK_CONTAINER(frame), view);

	bar = gtk_vscrollbar_new(NULL);
	gtk_widget_set_scroll_adjustments(view, NULL,
			gtk_range_get_adjustment(GTK_RANGE(bar)));
	gtk_box_pack_start(GTK_BOX(hbox), bar, FALSE, TRUE, 0);
	
	gtk_window_set_default_size(message_window,
				    gdk_screen_width() / 2,
				    gdk_screen_height() / 6);
	gtk_widget_show_all(GTK_WIDGET(message_window));

	gtk_widget_hide(log_window);
}

static gint log_clicked(GtkWidget *text, GdkEventButton *bev, gpointer data)
{
	if (bev->button == 1)
		gtk_widget_hide(log_window);
	else
		show_message_log();

	return 1;
}

static GList *show_log(Option *option, xmlNode *node, guchar *label)
{
	GtkWidget *button, *align;

	g_return_val_if_fail(option == NULL, NULL);
	
	align = gtk_alignment_new(0.5, 0.5, 0, 0);
	button = button_new_mixed(GTK_STOCK_YES, _("Show message log"));
	gtk_container_add(GTK_CONTAINER(align), button);
	gtk_signal_connect(GTK_OBJECT(button), "clicked",
			GTK_SIGNAL_FUNC(show_message_log), NULL);

	return g_list_append(NULL, align);
}
