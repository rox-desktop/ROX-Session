/*
 * $Id$
 *
 * ROX-Session, a very simple session manager
 * Copyright (C) 2004, Thomas Leonard, <tal197@users.sourceforge.net>.
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

/* This code invokes the setxkbmap command when the keymap is changed.
 * Code from Guido Schimmels.
 */

#include "config.h"

#include <string.h>
#include <stdio.h>
#include <glib.h>

#include <gdk/gdkx.h>
#include <X11/XKBlib.h>
#include <X11/extensions/XTest.h>
#include <X11/keysym.h>

#include "global.h"

#include "xkb.h"
#include "gui_support.h"

/* Used by settings.c */
void set_xkb_layout(const char *command)
{
	gchar **xkb_layout = NULL;	
	GPtrArray *argv;
	GError	*error = NULL;

	if (!command || !*command)
		return;

	argv = g_ptr_array_new();

	xkb_layout = g_strsplit(command, ";", 0);

	g_ptr_array_add(argv, "setxkbmap");

	if (!xkb_layout[0] || !xkb_layout[0][0])
			return;
	if (!xkb_layout[1] || !xkb_layout[1][0])
			return;
	g_ptr_array_add(argv, "-layout");
	g_ptr_array_add(argv, xkb_layout[1]);
	if (!xkb_layout[2] || strlen(xkb_layout[2]) == 0)
			return;
	g_ptr_array_add(argv, "-model");
	g_ptr_array_add(argv, xkb_layout[2]);
	if (xkb_layout[3])
	{
		if (xkb_layout[3][0])
		{
			g_ptr_array_add(argv, "-variant");
			g_ptr_array_add(argv, xkb_layout[3]);
		}
		if (xkb_layout[4])
		{
			if (xkb_layout[4][0])
			{
				g_ptr_array_add(argv, "-option");
				g_ptr_array_add(argv, xkb_layout[4]);
			}
		}
	}

	g_ptr_array_add(argv, NULL);

	g_spawn_async(NULL, (char **) (argv->pdata), NULL,
		G_SPAWN_SEARCH_PATH | G_SPAWN_STDOUT_TO_DEV_NULL,
		NULL, NULL, NULL, &error);

	if (error)
	{
		report_error(
			_("Failed to set keyboard map:\n%s\nYou can use "
			"the Keyboard configuration application to change the "
			"setting."),
			error->message);

		g_error_free(error);
	}

	g_ptr_array_free(argv, TRUE);
	g_strfreev(xkb_layout);
}

void set_xkb_repeat(gboolean repeat, int delay, int interval)
{
	if (repeat)
	{
		XAutoRepeatOn(GDK_DISPLAY());
		if (interval <= 0) interval = 1;
		if (delay <= 0) delay = 1;
		if (!XkbSetAutoRepeatRate(GDK_DISPLAY(), XkbUseCoreKbd,
					delay, interval))
			g_warning(_("Failed to set auto repeat rate/interval"));
	}
	else
	{
		XAutoRepeatOff(GDK_DISPLAY());
	}
}

/* Code for changing the led settings of a X display based on:
 * xsetleds (0.1.3) Copyright (C) 2002 Benedikt Meurer,
 * adapted for ROX-Session by Guido Schimmels <__guido__@web.de>.
*/

inline static gboolean get_xkb_state (const unsigned int code)
{
	unsigned int states;

	/* get key state using XKB */
	if (XkbGetIndicatorState (GDK_DISPLAY(), XkbUseCoreKbd, &states) != Success)
		g_warning (_("error in reading keyboard indicator states"));

	return (states & code) ? TRUE : FALSE;
}

static void toggle_xkb_state (const KeySym keysym, const gboolean keystate)
{
	/* toggle the state of the key by sending fake key events */

	if (get_xkb_state (keysym) != keystate)
	{
		KeyCode code = XKeysymToKeycode(GDK_DISPLAY(), keysym);
		if (!XTestFakeKeyEvent (GDK_DISPLAY(), code, True, CurrentTime) ||
			!XTestFakeKeyEvent (GDK_DISPLAY(), code, False, CurrentTime)) 
			g_warning (_("error while sending fake key events"));
	}
}

void set_xkb_numlock(const gboolean keystate)
{
	toggle_xkb_state (XK_Num_Lock, keystate);
}

void set_xkb_capslock(const gboolean keystate)
{
	toggle_xkb_state (XK_Caps_Lock, keystate);
}

void set_xkb_scrolllock(const gboolean keystate)
{
	toggle_xkb_state (XK_Scroll_Lock, keystate);
}

