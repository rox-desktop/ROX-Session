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

#include "wm.h"
#include "main.h"
#include "log.h"

#define MARGIN 10
#define TIME_SHOWN 8	/* Seconds that a message is displayed for */

static GtkWidget *log_window = NULL;
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

/* Static prototypes */
static void got_log_data(gpointer data,
			 gint source,
			 GdkInputCondition condition);
static gint log_clicked(GtkWidget *text, GdkEventButton *bev, gpointer data);
static void log_msg(guchar *text, gint len);

/****************************************************************
 *			EXTERNAL INTERFACE			*
 ****************************************************************/

void log_init(void)
{

	int fds[2];

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

	if (!chunks)
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
		gdk_window_move_resize(log_window->window, MARGIN, y,
				req.width, req.height);
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

	prune_time = time(NULL) - TIME_SHOWN;

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

static void log_msg(guchar *text, gint len)
{
	Chunk   *new;

	if (len < 0)
		len = strlen(text);

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
			log_msg("ROX-Session: write to stderr failed!\n", -1);
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
		log_msg("ROX-Session: read(stderr) failed!\n", -1);
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

static gint log_clicked(GtkWidget *text, GdkEventButton *bev, gpointer data)
{
	gtk_widget_hide(log_window);

	return 1;
}
