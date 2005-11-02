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

#if ROX_DBUS_VERSION >= 22000
#include <dbus/dbus-glib-lowlevel.h>
#endif

#define ROX_SESSION_ERROR "net.sf.rox.Session.Error"

/* DBus docs imply we should use a full path, but a comment in ROX-Session's
 * dbus.c implies it doesn't support multiple components
#define ROX_DBUS_PATH "net/sf/rox"
 */
#define ROX_DBUS_PATH ""

typedef DBusMessage *(*MessageHandler)(DBusMessage *message, DBusError *error);

/* Prototypes */
void dbus_init(void);
gboolean register_object_path(const char *path, MessageHandler handler);

#endif /* _DBUS_H */
