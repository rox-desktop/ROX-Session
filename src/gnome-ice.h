/*
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

/*
 * This file:
 *
 * Copyright (C) 1997, 1998, 1999, 2000 Free Software Foundation
 * All rights reserved.
 *
 * This file was stolen from GNOME.
 *
 * It was LGPL, but this copy is now GPL, as allowed by the LGPL.
 */

/* gnome-ice.h - Interface between ICE and Gtk.
   Written by Tom Tromey <tromey@cygnus.com>.  */

#ifndef GNOME_ICE_H
#define GNOME_ICE_H

/* This function should be called before any ICE functions are used.
   It will arrange for ICE connections to be read and dispatched via
   the Gtk event loop.  This function can be called any number of
   times without harm.  */
void gnome_ice_init (void);

#endif /* GNOME_ICE_H */
