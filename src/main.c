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

#include "config.h"

#include <stdlib.h>
#include <stdio.h>
#include <signal.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <errno.h>

#include <gtk/gtk.h>
#include <gtk/gtkinvisible.h>
#include <gdk/gdkx.h>
#include <X11/X.h>
#include <X11/Xatom.h>

#ifdef HAVE_GETOPT_LONG
#  include <getopt.h>
#endif

#include "main.h"
#include "gui_support.h"

#define COPYING								\
	     N_("Copyright (C) 2000 Thomas Leonard.\n"			\
		"ROX-Session comes with ABSOLUTELY NO WARRANTY,\n"	\
		"to the extent permitted by law.\n"			\
		"You may redistribute copies of ROX-Session\n"		\
		"under the terms of the GNU General Public License.\n"	\
		"For more information about these matters, "		\
		"see the file named COPYING.\n")

#ifdef HAVE_GETOPT_LONG
#  define USAGE   N_("Try `ROX-Session/AppRun --help' for more information.\n")
#  define SHORT_ONLY_WARNING ""
#else
#  define USAGE   N_("Try `ROX-Session/AppRun -h' for more information.\n")
#  define SHORT_ONLY_WARNING	\
		_("NOTE: Your system does not support long options - \n" \
		"you must use the short versions instead.\n\n")
#endif

#define HELP N_("Usage: ROX-Session/AppRun [OPTION]...\n"	\
       "If ROX-Session is already managing your session then display\n" \
       "the logout window. If not, offer to make ROX-Session your\n"	\
       "session manager for future sessions.\n\n"			\
       "  -h, --help		display this help and exit\n"		\
       "  -v, --version		display the version information and exit\n"   \
       "  -w, --wait		simply wait until run again (internal use)\n" \
       "\nThe latest version can be found at:\n"			\
       "\thttp://rox.sourceforge.net\n"					\
       "\nReport bugs to <tal197@users.sourceforge.net>.\n")

#define SHORT_OPS "hvw"

#ifdef HAVE_GETOPT_LONG
static struct option long_opts[] =
{
	{"help", 0, NULL, 'h'},
	{"version", 0, NULL, 'v'},
	{"wait", 0, NULL, 'w'},
	{NULL, 0, NULL, 0},
};
#endif

static GdkAtom rox_session_window;
static GtkWidget *ipc_window;

static guchar *app_dir;

/* Static prototypes */
static GdkWindow *get_existing_session();
static gboolean get_session(GdkWindow *window, Window *r_xid);
static void touch(GdkWindow *window);
static gboolean session_prop_touched(GtkWidget *window,
				     GdkEventProperty *event,
				     gpointer data);
static int become_default_session(void);

int main(int argc, char **argv)
{
	GdkWindowPrivate	*window;
	GdkWindow		*existing_session_window;
	struct sigaction	act;
	gboolean		wait_mode = FALSE;

	app_dir = g_strdup(getenv("APP_DIR"));

	if (!app_dir)
	{
		g_warning("APP_DIR environment variable was unset!\n"
			"Use the AppRun script to invoke ROX-Session...\n");
		app_dir = g_get_current_dir();
	}
#ifdef HAVE_UNSETENV 
	else
	{
		/* Don't pass it on to our child processes... */
		unsetenv("APP_DIR");
	}
#endif

	gtk_init(&argc, &argv);

	while (1)
	{
		int	c;
#ifdef HAVE_GETOPT_LONG
		int	long_index;
		c = getopt_long(argc, argv, SHORT_OPS,
				long_opts, &long_index);
#else
		c = getopt(argc, argv, SHORT_OPS);
#endif

		if (c == EOF)
			break;		/* No more options */
		
		switch (c)
		{
			case 'v':
				fprintf(stderr, "ROX-Session %s\n", VERSION);
				fputs(_(COPYING), stderr);
				return EXIT_SUCCESS;
			case 'h':
				fputs(_(HELP), stderr);
				fputs(_(SHORT_ONLY_WARNING), stderr);
				return EXIT_SUCCESS;
			case 'w':
				wait_mode = TRUE;
				break;
			default:
				fputs(_(USAGE), stderr);
				return EXIT_FAILURE;
		}
	}

	rox_session_window = gdk_atom_intern("_ROX_SESSION_WINDOW2", FALSE);

	existing_session_window = get_existing_session();
	if (existing_session_window)
	{
		/* There is a valid session already running... */

		if (wait_mode)
		{
			report_error(PROJECT,
				_("ROX-Session is already managing your "
				"session - can't manage it twice!"));
			return EXIT_FAILURE;
		}

		/* Tell it to logout  */
		touch(existing_session_window);
		return EXIT_SUCCESS;
	}

	if (!wait_mode)
		return become_default_session();

	/* Ignore dying children */
	act.sa_handler = SIG_IGN;
	sigemptyset(&act.sa_mask);
	act.sa_flags = SA_NOCLDSTOP;
	sigaction(SIGCHLD, &act, NULL);
	
	ipc_window = gtk_invisible_new();
	gtk_widget_realize(ipc_window);
			
	window = (GdkWindowPrivate *) ipc_window->window;
	gdk_property_change(ipc_window->window, rox_session_window,
			XA_WINDOW, 32, GDK_PROP_MODE_REPLACE,
			(guchar *) &window->xwindow, 1);
	gtk_widget_add_events(ipc_window, GDK_PROPERTY_CHANGE_MASK);
	gtk_signal_connect(GTK_OBJECT(ipc_window), "property-notify-event",
			GTK_SIGNAL_FUNC(session_prop_touched), NULL);
	gdk_property_change(GDK_ROOT_PARENT(), rox_session_window,
			XA_WINDOW, 32, GDK_PROP_MODE_REPLACE,
			(guchar *) &window->xwindow, 1);

	gtk_main();

	return EXIT_SUCCESS;
}

static GdkWindow *get_existing_session()
{
	Window		xid, xid_confirm;
	GdkWindow	*window;

	if (!get_session(GDK_ROOT_PARENT(), &xid))
		return NULL;

	window = gdk_window_foreign_new(xid);
	if (!window)
		return NULL;

	if (!get_session(window, &xid_confirm) || xid_confirm != xid)
		return NULL;

	return window;
}

static gboolean get_session(GdkWindow *window, Window *r_xid)
{
	guchar		*data;
	gint		format, length;
	gboolean	retval = FALSE;
	
	if (gdk_property_get(window, rox_session_window, XA_WINDOW, 0, 4,
			FALSE, NULL, &format, &length, &data) && data)
	{
		if (format == 32 && length == 4)
		{
			retval = TRUE;
			*r_xid = *((Window *) data);
		}
		g_free(data);
	}

	return retval;
}

static void touch(GdkWindow *window)
{
	gdk_property_change(window, rox_session_window, XA_WINDOW, 32,
			GDK_PROP_MODE_APPEND, "", 0);
}

static gboolean session_prop_touched(GtkWidget *window,
				     GdkEventProperty *event,
				     gpointer data)
{
	static gint logout_window_open = 0;

	if (event->atom == rox_session_window)
	{
		if (logout_window_open)
		{
			gdk_beep();
			return TRUE;
		}

		logout_window_open++;
		if (get_choice(PROJECT,
				_("Really logout?"), 2,
				_("Logout"), _("Cancel")) == 0)
		{
			gtk_main_quit();
		}
		logout_window_open--;
		
		return TRUE;
	}
	return FALSE;
}

static int become_default_session(void)
{
	int	status;
	pid_t	child;
	guchar	*path;

	if (get_choice(PROJECT,
		_("Would you like to make ROX-Session your session "
		  "manager?"), 2, _("Yes"), _("No")) != 0)
		return EXIT_SUCCESS;

	child = fork();

	switch (child)
	{
		case -1:
			report_error(PROJECT,
				_("fork() failed: giving up!"));
			return EXIT_FAILURE;
		case 0:
			/* We're the child... */
			path = g_strconcat(app_dir, "/",
					   "MakeDefault.sh", NULL);
			execl(path, path, app_dir, NULL);
			fprintf(stderr, "execl(%s) failed: %s\n", path,
					g_strerror(errno));
			fflush(stderr);
			_exit(1);
	}

	if (waitpid(child, &status, 0) != child)
	{
		g_error("waitpid(%d) failed: %s\n",
				child, g_strerror(errno));
	}

	if (WIFEXITED(status) && WEXITSTATUS(status) == 0)
	{
		report_error(PROJECT,
			_("OK, now logout by your usual method and when "
			"you log in again, I should be your session manager.\n"
			"You can edit your .xsession file to customise "
			"things..."));
	}
	else
	{
		report_error(PROJECT,
			_("Oh dear; it didn't work and I don't know why!\n"
			"Make sure your .xsession and .xinitrc files are OK, "
			"then report the problem to "
			"tal197@users.sourceforge.net - thanks"));

		return EXIT_FAILURE;
	}

	return EXIT_SUCCESS;
}
