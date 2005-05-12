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

#include "main.h"
#include "session.h"
#include "log.h"
#include "options.h"
#include "gui_support.h"

#define MARGIN 4

static Option o_time_shown;
static Option o_percent_switch;

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
#define MAX_BUFFER_SIZE 100000			/* In characters */
static time_t last_log_time;			/* Time of timestamp */
static GtkTextBuffer *buffer = NULL;
static GtkWindow *message_window = NULL;	/* The non-popup window */

/* Static prototypes */
static void got_log_data(gpointer data,
			 gint source,
			 GdkInputCondition condition);
static gint log_clicked(GtkWidget *text, GdkEventButton *bev, gpointer data);
static void log_msg(const gchar *text, gint len);
static GList *show_log(Option *option, xmlNode *node, guchar *label);
void show_message_log(void);
static void log_own_errors(const gchar *log_domain,
			   GLogLevelFlags log_level,
			   const gchar *message,
			   gpointer user_data);

/****************************************************************
 *			EXTERNAL INTERFACE			*
 ****************************************************************/

void log_init(void)
{
	GtkTextIter end;
	int fds[2];

	buffer = gtk_text_buffer_new(NULL);
	gtk_text_buffer_create_tag(buffer,
				"time", "foreground", "blue",
				NULL);
	time(&last_log_time);
	gtk_text_buffer_get_end_iter(buffer, &end);
	gtk_text_buffer_insert(buffer, &end, "ROX-Session started: ", -1);
	gtk_text_buffer_insert_with_tags_by_name(buffer, &end,
				ctime(&last_log_time), -1,
				"time", NULL);

	option_add_int(&o_time_shown, "log_time_shown", 5);
	option_add_int(&o_percent_switch, "log_percent_switch", 30);

	option_register_widget("show-log", show_log);

	if (pipe(fds))
	{
		g_warning("pipe(): %s\n", g_strerror(errno));
		return;
	}

	/* Since writing to stderr will block until we're ready to
	 * read it, it's a good idea to get our own errors some other
	 * way (ROX-Session may deadlock due because it can't write the
	 * error until it's ready to read previous errors, and can't read
	 * previous errors until it's written the new one).
	 * Pango likes to spew errors if it can't find its fonts...
	 */
	g_log_set_handler("", G_LOG_LEVEL_WARNING | G_LOG_FLAG_FATAL
                     | G_LOG_FLAG_RECURSION, log_own_errors, NULL);
	g_log_set_handler("Gdk", G_LOG_LEVEL_WARNING | G_LOG_FLAG_FATAL
                     | G_LOG_FLAG_RECURSION, log_own_errors, NULL);
	g_log_set_handler("Gtk", G_LOG_LEVEL_WARNING | G_LOG_FLAG_FATAL
                     | G_LOG_FLAG_RECURSION, log_own_errors, NULL);

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

/* Chunks has changed. Update the label (or hide if nothing to show).
 * If there's too much stuff, open the log window.
 */
static void show(void)
{
	static gboolean top = FALSE;
	int	y;
	GString *text;
	GList	*next;
	int	last_non_space = -1;
	GdkRectangle mon_geom;

	if (message_window || !chunks || !o_time_shown.int_value)
	{
		gtk_widget_hide(log_window);
		return;
	}

	/* Always use monitor 0, keeps it simpler */
	if (gdk_screen_get_n_monitors(gdk_screen_get_default()) > 1)
	{
		gdk_screen_get_monitor_geometry(gdk_screen_get_default(), 0,
				&mon_geom);
	}
	else
	{
		mon_geom.x = mon_geom.y = 0;
		mon_geom.width = gdk_screen_width();
		mon_geom.height = gdk_screen_height();
	}
	if (!GTK_WIDGET_VISIBLE(log_window))
	{
		gdk_window_get_pointer(GDK_ROOT_PARENT(), NULL, &y, NULL);

		top = 2 * y > mon_geom.height + mon_geom.y;
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
		max_h = o_percent_switch.int_value * mon_geom.height / 100;

		if (req.height > max_h)
		{
			show_message_log();
			return;
		}

		if (top)
			y = mon_geom.y + MARGIN;
		else
			y = mon_geom.y + mon_geom.height - MARGIN - req.height;

		req.width = mon_geom.width - 2 * MARGIN;

		gtk_widget_set_uposition(log_window, mon_geom.x + MARGIN, y);
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
	time_t  now;

	if (len < 0)
		len = strlen(text);

	time(&now);

	/* Add to the long history buffer... */
	gtk_text_buffer_get_end_iter(buffer, &end);
	if (now - last_log_time > 60)
	{
		gtk_text_buffer_insert_with_tags_by_name(buffer,
						 &end, ctime(&now), -1,
						 "time", NULL);
		last_log_time = now;
	}
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

static void log_own_errors(const gchar *log_domain,
			   GLogLevelFlags log_level,
			   const gchar *message,
			   gpointer user_data)
{
	g_print("%s\n", message);
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

	got = read(source, buffer, BUFFER_SIZE);

	if (got <= 0)
	{
		gtk_input_remove(input_tag);
		log_msg(_("ROX-Session: read(stderr) failed!\n"), -1);
	}
	else
	{
		if (g_utf8_validate(buffer, got, NULL)) {
			log_msg(buffer, got);
		} else {
			char *fixed;

			fixed = g_convert_with_fallback(buffer, got,
					"utf-8", "iso-8859-1",
					"?", NULL, NULL, NULL);
			log_msg(fixed, -1);
			log_msg("(invalid UTF-8)\n", -1);
			g_free(fixed);
		}

		if (real_stderr != -1)
			write_stderr(buffer, got);
	}

	if (call_child_died)
		child_died_callback();
}

void show_message_log(void)
{
	GtkWidget *view, *dialog, *sw, *vbox, *hbox, *close_button;
	GdkRectangle mon_geom;

	if (message_window)
	{
		gtk_window_present(message_window);
		return;
	}

	/* Can't guarantee to open on same monitor as pointer, but most people
	 * will have monitors with matching resolutions */
	if (gdk_screen_get_n_monitors(gdk_screen_get_default()) > 1)
	{
		int mx, my;

		gdk_window_get_pointer(GDK_ROOT_PARENT(), &mx, &my, NULL);
		gdk_screen_get_monitor_geometry(gdk_screen_get_default(),
				gdk_screen_get_monitor_at_point(
					gdk_screen_get_default(), mx, my),
				&mon_geom);
	}
	else
	{
		mon_geom.x = mon_geom.y = 0;
		mon_geom.width = gdk_screen_width();
		mon_geom.height = gdk_screen_height();
	}

	dialog = gtk_window_new(GTK_WINDOW_TOPLEVEL);
	message_window = GTK_WINDOW(dialog);
	gtk_window_set_title(message_window, _("ROX-Session message log"));
	vbox = gtk_vbox_new(FALSE, 0);
	gtk_container_add(GTK_CONTAINER(dialog), vbox);
	hbox = gtk_hbox_new(FALSE, 0);
	gtk_box_pack_end(GTK_BOX(vbox), hbox, FALSE, FALSE, 8);
	close_button = gtk_button_new_from_stock(GTK_STOCK_CLOSE);
	gtk_box_pack_end(GTK_BOX(hbox), close_button, FALSE, FALSE, 18);
	g_signal_connect_swapped(close_button, "clicked",
			G_CALLBACK(gtk_widget_destroy), message_window);
	g_signal_connect(message_window, "destroy",
			G_CALLBACK(gtk_widget_destroyed), &message_window);

	view = gtk_text_view_new_with_buffer(buffer);
	gtk_text_view_set_editable(GTK_TEXT_VIEW(view), FALSE);
	gtk_text_view_set_cursor_visible(GTK_TEXT_VIEW(view), FALSE);
	gtk_text_view_set_wrap_mode(GTK_TEXT_VIEW(view), GTK_WRAP_WORD);
	gtk_widget_set_size_request(view, 400, 100);

	sw = gtk_scrolled_window_new(NULL, NULL);
	gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(sw),
	                               GTK_POLICY_AUTOMATIC,
	                               GTK_POLICY_AUTOMATIC);
	gtk_scrolled_window_set_shadow_type(GTK_SCROLLED_WINDOW(sw),
					GTK_SHADOW_IN);
	
	gtk_box_pack_start(GTK_BOX(vbox), sw, TRUE, TRUE, 0);
	gtk_container_add(GTK_CONTAINER(sw), view);

	gtk_window_set_default_size(message_window,
				    mon_geom.width / 2, mon_geom.height / 4);
	/* I thought 1/6 of screen height looked too small */

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
