/*
 * $Id$
 *
 * ROX-Session, a very simple session manager
 * Copyright (C) 2002, Thomas Leonard, <tal197@users.sourceforge.net>.
 * This file (C) 2004, Tony Houghton, <h@realh.co.uk>
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

#include "dpms.h"

#include <X11/Xmd.h>
# include <X11/extensions/dpms.h>


static gboolean dpms_check(Display *dpy)
{
	int event = 0, error = 0;

	if (!DPMSQueryExtension(dpy, &event, &error))
	{
		g_warning(_("DPMS extension not supported"));
		return FALSE;
	}
	if (!DPMSCapable(dpy))
	{
		g_warning(_("Display not capable of DPMS"));
		return FALSE;
	}
	return TRUE;
}

void dpms_set_times(Display *dpy, int standby, int suspend, int off)
{
	CARD16 old_standby, old_suspend, old_off;

	if (!dpms_check(dpy))
		return;

	if (standby < 10 || suspend < 10 || off < 10)
	{
		g_warning(_("DPMS timeout under 10 seconds, rounding up"));
		if (standby < 10) standby = 10;
		if (suspend < 10) suspend = 10;
		if (off < 10) off = 10;
	}
	if (!DPMSGetTimeouts(dpy, &old_standby, &old_suspend, &old_off))
	{
		g_warning(_("Unable to read previous DPMS timeouts"));
		old_standby = old_suspend = old_off = 0;
	}
	if (old_standby != standby || old_suspend != suspend || old_off != off)
	{
		if (!DPMSSetTimeouts(dpy, standby, suspend, off))
			g_warning(_("Unable to set DPMS timeouts"));
	}
}

void dpms_set_enabled(Display *dpy, gboolean enable)
{
	BOOL old_enabled = False;
	CARD16 old_power = 0;

	if (!dpms_check(dpy))
		return;
	if (!DPMSInfo(dpy, &old_power, &old_enabled))
	{
		g_warning(_("Unable to get DPMS state"));
		old_enabled = ! enable;
	}
	if (old_enabled != enable)
	{
		if (!(enable ? DPMSEnable(dpy) : DPMSDisable(dpy)))
			g_warning(_("Unable to set DPMS state"));
	}
}

