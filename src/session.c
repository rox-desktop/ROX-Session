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

/* If log.c gets any data and this is TRUE, it calls child_died_callback() */
gboolean call_child_died = FALSE;

/* See http://www.freedesktop.org/standards/xsettings/xsettings.html */
XSettingsManager *manager = NULL;

typedef struct _Setting Setting;

struct _Setting {
	gchar *name;
	gchar *default_value;
	Option option;
};

static Setting settings[] = {
	{"Net/DoubleClickTime", "250"},
	{"Net/CursorBlink", "1"},
	{"Net/CursorBlinkTime", "1200"},

	{"Gtk/CanChangeAccels", "1"},
	{"Gtk/FontName", "Sans 10"},
};

static Option mouse_accel_threshold;
static Option mouse_accel_factor;

#define N_SETTINGS (sizeof(settings) / sizeof(*settings))

/* Static prototypes */
static GList *build_mouse_tester(Option *option, xmlNode *node, guchar *label);
static void child_died(int signum);
static void terminate_xsettings(void *data);
static void xsettings_changed(void);
static void show_session_options(void);

/****************************************************************
 *			EXTERNAL INTERFACE			*
 ****************************************************************/

void session_init(void)
{
	struct sigaction	act;
	int i;

	/* Let child processes die */
	act.sa_handler = child_died;
	sigemptyset(&act.sa_mask);
	act.sa_flags = SA_NOCLDSTOP;
	sigaction(SIGCHLD, &act, NULL);

	if (xsettings_manager_check_running(gdk_display,
					    DefaultScreen(gdk_display)))
	{
		g_printerr("An XSETTINGS manager is already running. "
				"Not taking control of XSETTINGS...\n");
	}
	else
		manager = xsettings_manager_new(gdk_display,
						DefaultScreen(gdk_display),
					        terminate_xsettings, NULL);

	for (i = 0; i < N_SETTINGS; i++)
	{
		option_add_string(&settings[i].option,
				  settings[i].name,
				  settings[i].default_value);
	}

	option_add_int(&mouse_accel_threshold, "accel_threshold", 10);
	option_add_int(&mouse_accel_factor, "accel_factor", 20);

	option_register_widget("mouse-tester", build_mouse_tester);

	option_add_notify(xsettings_changed);

	xsettings_changed();
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

void show_main_window(void)
{
	static GtkWidget *window = NULL;
	GtkWidget *button;

	if (window)
	{
		gtk_window_present(GTK_WINDOW(window));
		return;
	}
	
	window = gtk_message_dialog_new(NULL, GTK_DIALOG_MODAL,
			 GTK_MESSAGE_QUESTION, GTK_BUTTONS_NONE,
			 _("\nReally logout?\n"));

	gtk_window_set_title(GTK_WINDOW(window), PROJECT);

	gtk_window_set_position(GTK_WINDOW(window), GTK_WIN_POS_CENTER);

	button = button_new_mixed(GTK_STOCK_PREFERENCES,
				_("Session _Settings"));
	gtk_widget_show(button);
	gtk_dialog_add_action_widget(GTK_DIALOG(window), button, 1);

	gtk_dialog_add_button(GTK_DIALOG(window),
			GTK_STOCK_CANCEL, GTK_RESPONSE_DELETE_EVENT);

	button = button_new_mixed(GTK_STOCK_QUIT, _("_Logout"));
	GTK_WIDGET_SET_FLAGS(button, GTK_CAN_DEFAULT);
	gtk_widget_show(button);
	gtk_dialog_add_action_widget(GTK_DIALOG(window),
			button, GTK_RESPONSE_YES);

	gtk_dialog_set_default_response(GTK_DIALOG(window), GTK_RESPONSE_YES);

	switch (gtk_dialog_run(GTK_DIALOG(window)))
	{
		case GTK_RESPONSE_YES:
			gtk_main_quit();
			break;
		case 1:
			show_session_options();
			break;
		default:
			break;
	}

	gtk_widget_destroy(window);
	window = NULL;
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

	argv[0] = choices_find_path_load("Login", "ROX-Session");
	if (!argv[0])
		argv[0] = g_strconcat(app_dir, "/Login", NULL);

	argv[1] = NULL;
	g_spawn_async(NULL, argv, NULL, G_SPAWN_DO_NOT_REAP_CHILD,
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

/* Also called for mouse settings... */
static void xsettings_changed(void)
{
	int i;

	XChangePointerControl(GDK_DISPLAY(), True, True,
				mouse_accel_factor.int_value, 10,
				mouse_accel_threshold.int_value);

	if (!manager)
		return;

	for (i = 0; i < N_SETTINGS; i++)
	{
		if (isdigit(settings[i].default_value[0]))
			xsettings_manager_set_int(manager,
					settings[i].name,
					settings[i].option.int_value);
		else
			xsettings_manager_set_string(manager,
					settings[i].name,
					settings[i].option.value);
	}

	xsettings_manager_notify(manager);
}

static void terminate_xsettings(void *data)
{
	g_warning("ROX-Session is no longer the XSETTINGS manager!");
}

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

static gboolean tester_clicked(GtkBin *button, GdkEventButton *event)
{
	GtkLabel *label = GTK_LABEL(button->child);

	if (event->type == GDK_BUTTON_PRESS)
		gtk_label_set_text(label, _("Single click"));
	else if (event->type == GDK_2BUTTON_PRESS)
		gtk_label_set_text(label, _("Double click!"));
	
	return 1;
}

static gboolean reset_tester(GtkBin *button)
{
	GtkLabel *label = GTK_LABEL(button->child);

	gtk_label_set_text(label, _("Double-click here to test mouse"));
	return 0;
}

static GList *build_mouse_tester(Option *option, xmlNode *node, guchar *label)
{
	GtkWidget *widget;

	g_return_val_if_fail(option == NULL, NULL);
	g_return_val_if_fail(label == NULL, NULL);

	widget = gtk_button_new_with_label("");
	g_signal_connect(widget, "button-press-event",
			 G_CALLBACK(tester_clicked), NULL);
	g_signal_connect(widget, "leave-notify-event",
			 G_CALLBACK(reset_tester), NULL);
	reset_tester(GTK_BIN(widget));

	return g_list_prepend(NULL, widget);
}

static void show_session_options(void)
{
	if (!manager)
		report_error(_("ROX-Session not managing XSettings, so changes "
				"will have no immediate effect..."));
	options_show();
}

