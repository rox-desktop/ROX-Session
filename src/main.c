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
#include <string.h>
#include <signal.h>
#include <unistd.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

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
#include "dbus.h"
#include "settings.h"

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

#define SHORT_OPS "htvwm"

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

guchar *app_dir;

int main(int argc, char **argv)
{
	gboolean		wait_mode = FALSE;
	gboolean		messages = FALSE;
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

	/* Close stdin. We don't need it, and it can cause problems if
	 * a child process wants a password, etc...
	 */
	{
		int fd;
		fd = open("/dev/null", O_RDONLY);
		if (fd > 0)
		{
			close(0);
			dup2(fd, 0);
			close(fd);
		}
	}

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
				printf("ROX-Session %s\n", VERSION);
				fputs(_(COPYING), stdout);
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
			case 'm':
				messages = TRUE;
				break;
			default:
				fputs(_(USAGE), stderr);
				return EXIT_FAILURE;
		}
	}

	rc_file = choices_find_path_load("Styles", "ROX-Session");
	if(!rc_file)
		rc_file = g_strconcat(app_dir, "/Styles", NULL);
	gtk_rc_parse(rc_file);
	g_free(rc_file);

	if (!wait_mode)
	{
		report_error("Invalid options passed to ROX-Session");
		return EXIT_FAILURE;
	}

	dbus_init();

	session_init();

	log_init();		/* Capture standard error */

	settings_init();

	start_window_manager();

	if (test_mode)
	{
		/* show_main_window(); */
		system("echo $DBUS_SESSION_BUS_ADDRESS");
	}

	gtk_main();

	if (xsettings_manager)
		xsettings_manager_destroy(xsettings_manager);

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

