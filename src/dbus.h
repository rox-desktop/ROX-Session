/*
 * $Id$
 *
 * ROX-Session, a very simple session manager
 * By Thomas Leonard, <tal197@users.sourceforge.net>.
 */

#ifndef _DBUS_H
#define _DBUS_H

#define DBUS_API_SUBJECT_TO_CHANGE
#include <dbus/dbus.h>
#include <dbus/dbus-glib.h>

#ifdef DBUS_03
#include <dbus/dbus-glib-lowlevel.h>
#endif

#define ROX_SESSION_ERROR "net.sf.rox.Session.Error"

typedef DBusMessage *(*MessageHandler)(DBusMessage *message, DBusError *error);

/* Prototypes */
void dbus_init(void);
gboolean register_object_path(const char *path, MessageHandler handler);

#endif /* _DBUS_H */
