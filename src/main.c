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

#include <stdlib.h>
#include <stdio.h>
#include <signal.h>
#include <unistd.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <errno.h>

#include <gdk/gdkx.h>
#include <gtk/gtk.h>
#include <gtk/gtkinvisible.h>
#include <X11/X.h>
#include <X11/Xatom.h>

#ifdef HAVE_GETOPT_LONG
#  include <getopt.h>
#endif

#include "main.h"
#include "wm.h"
#include "log.h"
#include "gui_support.h"
#include "choices.h"
#include "wm.h"
#include "xsettings-manager.h"
#include "session.h"
#include "options.h"

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

#define SHORT_OPS "htvw"

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

guchar *app_dir;

/* The pid of the Login script. -1 if terminated. If this process
 * exits with a non-zero exit status then we assume the session went
 * wrong and try to fix it.
 */
static pid_t	login_child = -1;
int		login_error = 0;	/* Non-zero => error */

/* Static prototypes */
static GdkWindow *get_existing_session();
static gboolean get_session(GdkWindow *window, Window *r_xid);
static void touch(GdkWindow *window);
static gboolean session_prop_touched(GtkWidget *window,
				     GdkEventProperty *event,
				     gpointer data);
static int become_default_session(void);
static void run_login_script(void);

/* This is called as a signal handler */
static void child_died(int signum)
{
	gboolean notify = FALSE;
	pid_t	child;
	int	status;

	/* Find out which children exited and allow them to die */
	while (1)
	{
		child = waitpid(-1, &status, WNOHANG);

		if (child == 0 || child == -1)
			break;

		if (child == wm_pid)
		{
			wm_pid = -2;
			notify = TRUE;
		}

		if (child == login_child &&
			(WIFEXITED(status) == 0 || WEXITSTATUS(status) != 0))
		{
			/* Wake up! Use non-blocking I/O in case the pipe is
			 * already full (if so, we'll catch the flag later).
			 */
			login_child = -1;
			login_error = WIFEXITED(status) == 0
					? -1	/* Signal death */
					: WEXITSTATUS(status);

			notify = TRUE;
		}
	}

	if (notify)
	{
		fcntl(STDERR_FILENO, O_NONBLOCK, TRUE);
		write(STDERR_FILENO, "\n", 1);
	}
}

int main(int argc, char **argv)
{
	Window			xwindow;
	GdkWindow		*window;
	GdkWindow		*existing_session_window;
	struct sigaction	act;
	gboolean		wait_mode = FALSE;
	gboolean		test_mode = FALSE;
	guchar			*rc_file;

	app_dir = g_strdup(getenv("APP_DIR"));

	if (!app_dir)
		g_error("APP_DIR environment variable was unset!\n"
			"Use the AppRun script to invoke ROX-Session...\n");
#ifdef HAVE_UNSETENV 
	else
	{
		/* Don't pass it on to our child processes... */
		unsetenv("APP_DIR");
	}
#endif

	gtk_init(&argc, &argv);

	choices_init();
	options_init();

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
			case 't':
				test_mode = TRUE;
				break;
			case 'w':
				wait_mode = TRUE;
				break;
			default:
				fputs(_(USAGE), stderr);
				return EXIT_FAILURE;
		}
	}

	rc_file = g_strconcat(app_dir, "/Styles", NULL);
	gtk_rc_parse(rc_file);
	g_free(rc_file);

	rox_session_window = gdk_atom_intern(
			test_mode ? "_ROX_SESSION_TEST"
				  : "_ROX_SESSION_WINDOW2", FALSE);

	existing_session_window = get_existing_session();
	if (existing_session_window)
	{
		/* There is a valid session already running... */

		if (wait_mode)
		{
			report_error(_("ROX-Session is already managing your "
					"session - can't manage it twice!"));
			return EXIT_FAILURE;
		}

		/* Tell it to logout  */
		touch(existing_session_window);
		return EXIT_SUCCESS;
	}

	if (!wait_mode)
		return become_default_session();

	ipc_window = gtk_invisible_new();
	gtk_widget_realize(ipc_window);
			
	window = ipc_window->window;
	xwindow = GDK_WINDOW_XWINDOW(window);
	gdk_property_change(ipc_window->window, rox_session_window,
			gdk_x11_xatom_to_atom(XA_WINDOW), 32,
			GDK_PROP_MODE_REPLACE, (guchar *) &xwindow, 1);
	gtk_widget_add_events(ipc_window, GDK_PROPERTY_CHANGE_MASK);
	gtk_signal_connect(GTK_OBJECT(ipc_window), "property-notify-event",
			GTK_SIGNAL_FUNC(session_prop_touched), NULL);
	gdk_property_change(GDK_ROOT_PARENT(), rox_session_window,
			gdk_x11_xatom_to_atom(XA_WINDOW), 32,
			GDK_PROP_MODE_REPLACE, (guchar *) &xwindow, 1);

	/* Let child processes die */
	act.sa_handler = child_died;
	sigemptyset(&act.sa_mask);
	act.sa_flags = SA_NOCLDSTOP;
	sigaction(SIGCHLD, &act, NULL);

	session_init();

	log_init();		/* Capture standard error */

	if (test_mode)
	{
		show_main_window();
		system("rox -n&");
	}
	else
	{
		start_window_manager();

		run_login_script();
	}

	gtk_main();

	if (manager)
		xsettings_manager_destroy(manager);

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
	
	if (gdk_property_get(window, rox_session_window,
				gdk_x11_xatom_to_atom(XA_WINDOW), 0, 4,
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
	gdk_property_change(window, rox_session_window,
			gdk_x11_xatom_to_atom(XA_WINDOW), 32,
			GDK_PROP_MODE_APPEND, "", 0);
	gdk_flush();
}

static gboolean session_prop_touched(GtkWidget *window,
				     GdkEventProperty *event,
				     gpointer data)
{
	if (event->atom != rox_session_window)
		return FALSE;

	show_main_window();
		
	return TRUE;
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
			report_error(_("fork() failed: giving up!"));
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
		g_error("waitpid(%ld) failed: %s\n",
				(long) child, g_strerror(errno));
	}

	if (WIFEXITED(status) && WEXITSTATUS(status) == 0)
	{
		report_error( _("OK, now logout by your usual method and when "
			"you log in again, I should be your session manager.\n"
			"You can edit your .xsession file to customise "
			"things..."));
	}
	else
	{
		report_error(
			_("Oh dear; it didn't work and I don't know why!\n"
			"Make sure your .xsession and .xinitrc files are OK, "
			"then report the problem to "
			"tal197@users.sourceforge.net - thanks"));

		return EXIT_FAILURE;
	}

	return EXIT_SUCCESS;
}

void login_failure(int error)
{
	GString		*message;
	guchar		*login;

	login = choices_find_path_load("Login", "ROX-Session");
	if (!login)
		login = g_strconcat(app_dir, "/Login", NULL);

	message = g_string_new(NULL);

	if (error < 0)
		g_string_sprintf(message, _("Your login script (%s) died"),
					login);
	else
		g_string_sprintf(message, _("Your login script (%s) exited "
					    "with exit status %d"),
					login, error);

	g_string_append(message,
		_(". I'll give you an xterm to try and fix it. ROX-Session "
		  "itself is running fine though - run me a second time "
		  "to logout."));

	g_free(login);
	report_error("%s", message->str);
	g_string_free(message, TRUE);

	system("xterm&");
}

static void run_login_script(void)
{
	guchar	*login;
	guchar	*error = NULL;

	login = choices_find_path_load("Login", "ROX-Session");
	if (!login)
		login = g_strconcat(app_dir, "/Login", NULL);

	login_child = fork();

	switch (login_child)
	{
		case -1:
			error = g_strdup(
				_("fork() failed - can't run Login script!"));
			break;
		case 0:
			execl(login, login, NULL);
			g_warning("exec(%s) failed: %s\n",
					login, g_strerror(errno));
			_exit(1);
	}

	g_free(login);
}
