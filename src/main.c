/* vi: set cindent:
 * $Id$
 *
 * ROX-Session, a very simple session manager
 * By Thomas Leonard, <tal197@ecs.soton.ac.uk>.
 */

#include <stdlib.h>
#include <stdio.h>
#include <signal.h>

#include <gtk/gtk.h>
#include <gdk/gdkx.h>
#include <X11/X.h>
#include <X11/Xatom.h>

static GdkAtom rox_session_window;
static GtkWidget *logout_window;

/* Static prototypes */
static gint logout_window_close(GtkWidget *window, gpointer data);
static GtkWidget *create_logout_window();
static GdkWindow *get_existing_session();
static gboolean get_session(GdkWindow *window, Window *r_xid);
static void touch(GdkWindow *window);
static gboolean session_prop_touched(GtkWidget *window,
				     GdkEventProperty *event,
				     gpointer data);

int main(int argc, char **argv)
{
	GdkWindowPrivate	*window;
	GdkWindow		*existing_session_window;
	struct sigaction	act = {};

	gtk_init(&argc, &argv);

	rox_session_window = gdk_atom_intern("_ROX_SESSION_WINDOW", FALSE);

	existing_session_window = get_existing_session();
	if (existing_session_window)
	{
		/* There is a valid session already running...
		 * Tell it to logout.
		 */
		touch(existing_session_window);
		return EXIT_SUCCESS;
	}

	/* Ignore dying children */
	act.sa_handler = SIG_IGN;
	sigemptyset(&act.sa_mask);
	act.sa_flags = SA_NOCLDSTOP;
	sigaction(SIGCHLD, &act, NULL);
	
	logout_window = create_logout_window();
			
	window = (GdkWindowPrivate *) logout_window->window;
	gdk_property_change(logout_window->window, rox_session_window,
			XA_WINDOW, 32, GDK_PROP_MODE_REPLACE,
			(guchar *) &window->xwindow, 1);
	gtk_widget_add_events(logout_window, GDK_PROPERTY_CHANGE_MASK);
	gtk_signal_connect(GTK_OBJECT(logout_window), "property-notify-event",
			GTK_SIGNAL_FUNC(session_prop_touched), NULL);
	gdk_property_change(GDK_ROOT_PARENT(), rox_session_window,
			XA_WINDOW, 32, GDK_PROP_MODE_REPLACE,
			(guchar *) &window->xwindow, 1);

	gtk_main();

	return EXIT_SUCCESS;
}

static gint logout_window_close(GtkWidget *window, gpointer data)
{
	gtk_widget_hide(window);

	return TRUE;
}

static void logout(GtkWidget *widget, gpointer data)
{
	gtk_main_quit();
}

static void cancel(GtkWidget *widget, gpointer data)
{
	gtk_widget_hide(logout_window);
}

static GtkWidget *create_logout_window()
{
	GtkWidget	*window, *button, *actions, *vbox, *label, *sep;

	window = gtk_window_new(GTK_WINDOW_DIALOG);
	gtk_window_set_title(GTK_WINDOW(window), "ROX-Session");
	gtk_window_set_position(GTK_WINDOW(window), GTK_WIN_POS_CENTER);
	gtk_container_set_border_width(GTK_CONTAINER(window), 8);
	gtk_signal_connect(GTK_OBJECT(window), "delete_event",
			GTK_SIGNAL_FUNC(logout_window_close), NULL);

	vbox = gtk_vbox_new(FALSE, 0);
	gtk_container_add(GTK_CONTAINER(window), vbox);

	label = gtk_label_new("Really logout?");
	gtk_widget_set_usize(label, 200, 100);
	gtk_box_pack_start(GTK_BOX(vbox), label, TRUE, TRUE, 0);

	sep = gtk_hseparator_new();
	gtk_box_pack_start(GTK_BOX(vbox), sep, FALSE, FALSE, 8);

	actions = gtk_hbox_new(TRUE, 0);
	gtk_box_pack_start(GTK_BOX(vbox), actions, FALSE, FALSE, 0);
	
	button = gtk_button_new_with_label("Logout");
	gtk_box_pack_start(GTK_BOX(actions), button, TRUE, TRUE, 0);
	gtk_signal_connect(GTK_OBJECT(button), "clicked", logout, NULL);

	button = gtk_button_new_with_label("Cancel");
	gtk_box_pack_start(GTK_BOX(actions), button, TRUE, TRUE, 0);
	gtk_signal_connect(GTK_OBJECT(button), "clicked", cancel, NULL);
	
	gtk_widget_realize(window);
	return window;
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
	if (event->atom == rox_session_window)
	{
		if (GTK_WIDGET_MAPPED(logout_window))
			gtk_widget_hide(logout_window);
		gtk_widget_show_all(logout_window);
		return TRUE;
	}
	return FALSE;
}
