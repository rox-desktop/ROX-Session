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

#include "main.h"
#include "log.h"

#define MARGIN 10
#define TIME_SHOWN 4	/* Seconds that a message is displayed for */

static GtkWidget *log_window = NULL;
static GtkWidget *da = NULL;
static gint	 input_tag;
static gint	 line_width;	/* Chars per line */

/* Static prototypes */
static void got_log_data(gpointer data,
			 gint source,
			 GdkInputCondition condition);
static gint log_clicked(GtkWidget *text, GdkEventButton *bev, gpointer data);
static gboolean expose(GtkWidget *widget, GdkEventExpose *event, gpointer data);

/****************************************************************
 *			EXTERNAL INTERFACE			*
 ****************************************************************/

void log_init(void)
{
	GdkFont	*font;

	int fds[2];

	if (pipe(fds))
	{
		g_warning("pipe(): %s\n", g_strerror(errno));
		return;
	}

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
	gtk_widget_realize(log_window);
	gdk_window_set_override_redirect(log_window->window, TRUE);

	/* We could use a GtkText widget, but it seems to be very broken... */
	da = gtk_drawing_area_new();
	gtk_widget_add_events(da, GDK_BUTTON_PRESS_MASK);
	gtk_signal_connect(GTK_OBJECT(da), "button_press_event",
			GTK_SIGNAL_FUNC(log_clicked), NULL);
	gtk_signal_connect(GTK_OBJECT(da), "expose-event",
			GTK_SIGNAL_FUNC(expose), NULL);

	gtk_container_add(GTK_CONTAINER(log_window), da);

	gtk_widget_show(da);

	font = da->style->font;
	line_width = (gdk_screen_width() - 2 * MARGIN) /
			gdk_string_width(font, "m");	/* Fixed font */

	input_tag = gdk_input_add(fds[0], GDK_INPUT_READ, got_log_data, NULL);
}

/****************************************************************
 *			INTERNAL FUNCTIONS			*
 ****************************************************************/

/* Keep these three in sync! */
static GList	*lines = NULL;		/* List of (guchar *) lines of text. */
static GList	*line_times = NULL;	/* time() of insertion */
static guint	nlines = 0;

/* Return the data for the first list element and remove the element */
static gpointer shift(GList **list)
{
	GList	*first = *list;
	gpointer retval;

	*list = g_list_remove_link(first, first);

	retval = first->data;
	g_list_free(first);

	return retval;
}

static void show(void)
{
	static gboolean top;
	GdkFont	*font = da->style->font;
	int	font_height = font->ascent + font->descent;
	int	h, y;

	if (nlines == 0)
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

	if (top)
		y = MARGIN;
	else
		y = h - MARGIN - nlines * font_height;

	gtk_widget_set_uposition(log_window, MARGIN, y);
	gdk_window_move_resize(log_window->window, MARGIN, y,
			gdk_screen_width() - 2 * MARGIN,
			nlines * font_height);
	gtk_widget_show(log_window);

	gdk_window_raise(log_window->window);

	expose(da, NULL, NULL);
}

/* Remove old entries */
static gboolean prune(gpointer data)
{
	static  gint prune_timeout = 0;

	GdkFont	*font = da->style->font;
	int	font_height = font->ascent + font->descent;
	gboolean did_prune = FALSE;
	time_t	prune_time;

	prune_time = time(NULL) - TIME_SHOWN;

	while (nlines)
	{
		if (nlines * font_height < gdk_screen_height() / 3)
		{
			time_t	*then = (time_t *) line_times->data;
			
			if (*then > prune_time)
				break;
		}
			
		nlines--;
		g_free(shift(&lines));
		g_free(shift(&line_times));

		did_prune = TRUE;
	}

	if (did_prune)
	{
		gdk_window_clear(da->window);
		show();
	}

	if (nlines)
	{
		if (prune_timeout)
			gtk_timeout_remove(prune_timeout);
		prune_timeout = gtk_timeout_add(1000, prune, NULL);
	}

	return FALSE;
}

static void log_msg(guchar *text, gint len)
{
	static GString *buffer = NULL;
	gboolean added = FALSE;
	gchar	*nl;
	time_t	now;

	time(&now);

	if (!buffer)
		buffer = g_string_new(NULL);
	
	if (len < 0)
		g_string_append(buffer, text);
	else
	{
		guchar	*tmp;

		tmp = g_strndup(text, len);
		g_string_append(buffer, tmp);
		g_free(tmp);
	}

	while ((nl = strchr(buffer->str, '\n')) || buffer->len >= line_width)
	{
		/* Are we breaking on a \n or on line_width? */
		gboolean got_nl = nl && nl - buffer->str <= line_width;
		int	len = got_nl ? nl - buffer->str : line_width;

		if (len > 0)
		{
			added = TRUE;
			lines = g_list_append(lines,
					g_strndup(buffer->str, len));
			line_times = g_list_append(line_times,
					g_memdup(&now, sizeof(now)));
			nlines++;
		}

		g_string_erase(buffer, 0, got_nl ? len + 1 : len);
	}

	if (added)
	{
		prune(NULL);
		show();
	}
}

static gboolean expose(GtkWidget *widget, GdkEventExpose *event, gpointer data)
{
	GdkFont	*font = widget->style->font;
	GdkWindow *win = widget->window;
	GdkGC	*gc = widget->style->fg_gc[GTK_STATE_NORMAL];
	GList	*next;
	int	y = font->ascent;

	for (next = lines; next; next = next->next)
	{
		guchar	*line = (guchar *) next->data;

		gdk_draw_string(win, font, gc, 0, y, line);

		y += font->ascent + font->descent;
	}

	return TRUE;
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
		log_msg("ROX-Session: read(stderr) failed!\n", -1);
	}
	else
		log_msg(buffer, got);

	if (error)
		login_failure(error);
}

static gint log_clicked(GtkWidget *text, GdkEventButton *bev, gpointer data)
{
	gtk_widget_hide(log_window);

	return 1;
}
