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

#include <errno.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <ctype.h>
#include <signal.h>
#include <sys/param.h>
#include <limits.h>
#include <stdlib.h>
#include <sys/types.h>
#include <dirent.h>
#include <string.h>

#include <gdk/gdkx.h>
#include <gtk/gtk.h>

#include "global.h"

#include "session.h"
#include "gui_support.h"
#include "options.h"
#include "choices.h"
#include "main.h"
#include "wm.h"


/* The pid of the Login script. -1 if terminated. If this process
 * exits with a non-zero exit status then we assume the session went
 * wrong and try to fix it.
 */
static pid_t	login_child = -1;

/* The pid of the ROX-Filer process managing the pinboard and panel.
 * ROX-Session will offer to restart this if it dies.
 */
static pid_t	rox_pid = -1;

/* If log.c gets any data and this is TRUE, it calls child_died_callback() */
gboolean call_child_died = FALSE;

static Option halt_command, reboot_command, suspend_command;

static const char * bad_xpm[] = {
"12 12 3 1",
" 	c #000000000000",
".	c #FFFF00000000",
"x	c #FFFFFFFFFFFF",
"            ",
" ..xxxxxx.. ",
" ...xxxx... ",
" x...xx...x ",
" xx......xx ",
" xxx....xxx ",
" xxx....xxx ",
" xx......xx ",
" x...xx...x ",
" ...xxxx... ",
" ..xxxxxx.. ",
"            "};

#define ROX_STOCK_HALT "rox-halt"
#define ROX_STOCK_SUSPEND "rox-suspend"

static const char *stocks[] = {
	ROX_STOCK_HALT,
	ROX_STOCK_SUSPEND,
};

/* Static prototypes */
static void child_died(int signum);
static char *pathdup(const char *path);
static void rox_process_died(void);
static void run_rox_process(void);


/****************************************************************
 *			EXTERNAL INTERFACE			*
 ****************************************************************/

void session_init(void)
{
	GtkIconFactory *factory;
	struct sigaction	act;
	int i;

	/* Let child processes die */
	act.sa_handler = child_died;
	sigemptyset(&act.sa_mask);
	act.sa_flags = SA_NOCLDSTOP;
	sigaction(SIGCHLD, &act, NULL);

	option_add_string(&halt_command, "halt_command", "halt");
	option_add_string(&reboot_command, "reboot_command", "reboot");
	option_add_string(&suspend_command, "suspend_command",
			  "xset dpms force off");

	factory = gtk_icon_factory_new();
	for (i = 0; i < G_N_ELEMENTS(stocks); i++)
	{
		GdkPixbuf *pixbuf;
		GError *error = NULL;
		gchar *path;
		GtkIconSet *iset;
		const gchar *name = stocks[i];

		path = g_strconcat(app_dir, "/images/", name, ".png", NULL);
		pixbuf = gdk_pixbuf_new_from_file(path, &error);
		if (!pixbuf)
		{
			g_warning("%s", error->message);
			g_error_free(error);
			pixbuf = gdk_pixbuf_new_from_xpm_data(bad_xpm);
		}
		g_free(path);

		iset = gtk_icon_set_new_from_pixbuf(pixbuf);
		g_object_unref(G_OBJECT(pixbuf));
		gtk_icon_factory_add(factory, name, iset);
		gtk_icon_set_unref(iset);
	}
	gtk_icon_factory_add_default(factory);
}

/* Called from the mainloop sometime after child_died executes */
void child_died_callback(void)
{
	pid_t	child;
	int	status;

	call_child_died = FALSE;

	/* Find out which children exited and allow them to die */
	while (1)
	{
		child = waitpid(-1, &status, WNOHANG);

		if (child == 0 || child == -1)
			break;

		if (child == wm_pid)
		{
			wm_pid = -1;
			wm_process_died();
		}

		if (child == rox_pid)
		{
			rox_pid = -1;
			rox_process_died();
		}

		if (child == login_child &&
			(WIFEXITED(status) == 0 || WEXITSTATUS(status) != 0))
		{
			login_child = -1;
			login_failure(WIFEXITED(status) == 0
					? -1	/* Signal death */
					: WEXITSTATUS(status));
		}
	}
}

void run_login_script(void)
{
	static gboolean logged_in = FALSE;
	GError	*error = NULL;
	gchar	*argv[2];
	gint	pid;

	if (logged_in || test_mode)
		return;
	logged_in = TRUE;

	/* Run ROX-Filer */
	run_rox_process();

	/* Run Login script */

	argv[0] = choices_find_path_load("Login", "ROX-Session");
	if (!argv[0])
		argv[0] = g_strconcat(app_dir, "/Login", NULL);

	argv[1] = NULL;
	g_spawn_async(NULL, argv, NULL,
			G_SPAWN_DO_NOT_REAP_CHILD | G_SPAWN_STDOUT_TO_DEV_NULL,
			NULL, NULL, &pid, &error);
	g_free(argv[0]);

	if (error)
	{
		login_child = -1;
	
		report_error(_("Failed to run login script:\n%s"),
				error->message);
		
		g_error_free(error);
	}
	else
		login_child = pid;
}

/****************************************************************
 *			INTERNAL FUNCTIONS			*
 ****************************************************************/

/* This is called as a signal handler.
 * Don't do the waitpid here, because this might get called before
 * the assignment in child = fork() completes!
 */
static void child_died(int signum)
{
	fcntl(STDERR_FILENO, O_NONBLOCK, TRUE);
	write(STDERR_FILENO, "\n", 1);
	call_child_died = TRUE;
}

void show_session_options(void)
{
	options_show();
}

/* Like g_strdup, but does realpath() too (if possible) */
static char *pathdup(const char *path)
{
	char real[MAXPATHLEN];

	g_return_val_if_fail(path != NULL, NULL);

	if (realpath(path, real))
		return g_strdup(real);

	return g_strdup(path);
}

static void rox_process_died(void)
{
	int r;

	rox_pid = -1;

	r = get_choice(_("ROX-Filer has terminated (crashed?)."
			 "You should probably try to restart it."), 3,
			GTK_STOCK_NO, _("Do nothing"),
			GTK_STOCK_EXECUTE, _("Run Xterm"),
			GTK_STOCK_REFRESH, _("_Restart"));

	if (r == 2)
		run_rox_process();
	else if (r == 1)
		system("xterm &");
}

static void run_rox_process(void)
{
	GError	*error = NULL;
	gchar	*argv[3];
	gint	pid;

	argv[0] = choices_find_path_load("RunROX", "ROX-Session");
	if (!argv[0])
		argv[0] = g_strconcat(app_dir, "/RunROX", NULL);
	argv[1] = pathdup(app_dir);
	argv[2] = NULL;
	g_spawn_async(NULL, argv, NULL,
			G_SPAWN_DO_NOT_REAP_CHILD | G_SPAWN_STDOUT_TO_DEV_NULL,
			NULL, NULL, &pid, &error);

	if (error)
	{
		rox_pid = -1;
		report_error(_("Failed to run '%s':\n%s"),
				argv[0], error->message);
		g_error_free(error);
		rox_process_died();
	}
	else
		rox_pid = pid;

	g_free(argv[0]);
	g_free(argv[1]);
}
