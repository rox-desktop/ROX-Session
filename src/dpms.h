/*
 * $Id$
 *
 * ROX-Session, a very simple session manager
 * By Thomas Leonard, <tal197@users.sourceforge.net>.
 * This file by Tony Houghton, <h@realh.co.uk>
 */

#ifndef _DPMS_H
#define _DPMS_H

#include "config.h"
#include <X11/Xlib.h>
#include <glib.h>

void dpms_set_times(Display *dpy, int standby, int suspend, int off);

void dpms_set_enabled(Display *dpy, gboolean enable);

#endif /* _DPMS_H */
