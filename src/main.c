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
#include <errno.h>

#include <gdk/gdkx.h>
#include <gtk/gtk.h>
#include <gtk/gtkinvisible.h>
#include <X11/X.h>
#include <X11/Xatom.h>

#ifdef HAVE_GETOPT_LONG
#  include <getopt.h>
#endif

#include "global.h"

#include "main.h"
#include "wm.h"
#include "log.h"
#include "gui_support.h"
#include "choices.h"
#include "wm.h"
#include "xsettings-manager.h"
#include "session.h"
#include "options.h"
#include "i18n.h"

#define COPYING								\
	     N_("Copyright (C) 2002 Thomas Leonard.\n"			\
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

gboolean test_mode = FALSE;

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
	Window			xwindow;
	GdkWindow		*window;
	GdkWindow		*existing_session_window;
	gboolean		wait_mode = FALSE;
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
	i18n_init();

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
	gtk_signal_connect(GTK_OBJECT(ipc_window), "property-notify-event",
			GTK_SIGNAL_FUNC(session_prop_touched), NULL);
	gtk_widget_add_events(ipc_window, GDK_PROPERTY_CHANGE_MASK);
	gdk_property_change(ipc_window->window, rox_session_window,
			gdk_x11_xatom_to_atom(XA_WINDOW), 32,
			GDK_PROP_MODE_REPLACE, (guchar *) &xwindow, 1);
	gdk_property_change(GDK_ROOT_PARENT(), rox_session_window,
			gdk_x11_xatom_to_atom(XA_WINDOW), 32,
			GDK_PROP_MODE_REPLACE, (guchar *) &xwindow, 1);

	session_init();

	log_init();		/* Capture standard error */
	start_window_manager();

	if (test_mode)
	{
		show_main_window();
		/* system("rox -n&"); */
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
	static gboolean first_time = TRUE;

	if (event->atom != rox_session_window)
		return FALSE;

	if (first_time)
	{
		/* Simply selecting for property events AFTER setting it
		 * the first time doesn't work, because Gtk might have
		 * selected them itself by then.
		 */
		first_time = FALSE;
		return FALSE;
	}

	show_main_window();
		
	return TRUE;
}

static int become_default_session(void)
{
	char	*argv[3];
	int	status;
	GError	*error = NULL;
	GtkWidget *dialog;

	dialog = gtk_message_dialog_new(NULL, GTK_DIALOG_MODAL,
		GTK_MESSAGE_QUESTION, GTK_BUTTONS_YES_NO,
		_("Would you like to make ROX-Session your session manager?"));
	gtk_window_set_position(GTK_WINDOW(dialog), GTK_WIN_POS_CENTER);
	gtk_dialog_set_default_response(GTK_DIALOG(dialog), GTK_RESPONSE_YES);
	if (gtk_dialog_run(GTK_DIALOG(dialog)) != GTK_RESPONSE_YES)
		return EXIT_SUCCESS;
	gtk_widget_destroy(dialog);

	argv[0] = g_strconcat(app_dir, "/", "MakeDefault.sh", NULL);
	argv[1] = app_dir;
	argv[2] = NULL;

	g_spawn_sync(NULL, argv, NULL, 0, NULL, NULL,
				NULL, NULL, &status, &error);

	g_free(argv[0]);

	if (error)
	{
		report_error(
			_("Oh dear; it didn't work and I don't know why!\n"
			"Error was:\n%s\n"
			"Make sure your .xsession and .xinitrc files are OK, "
			"then report the problem to "
			"tal197@users.sourceforge.net - thanks"),
			error->message);
		g_error_free(error);
		return EXIT_SUCCESS;
	}

	dialog = gtk_message_dialog_new(NULL, GTK_DIALOG_MODAL,
			GTK_MESSAGE_INFO, GTK_BUTTONS_OK,
			_("OK, now logout by your usual method and when "
			"you log in again, I should be your session manager.\n"
			"You can edit your .xsession file to customise "
			"things..."));
	gtk_window_set_position(GTK_WINDOW(dialog), GTK_WIN_POS_CENTER);
	gtk_dialog_run(GTK_DIALOG(dialog));
	gtk_widget_destroy(dialog);

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

