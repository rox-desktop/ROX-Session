/*
 * $Id$
 *
 * ROX-Session, a very simple session manager
 * By Thomas Leonard, <tal197@users.sourceforge.net>.
 */

#ifndef _SESSION_H
#define _SESSION_H

#include <X11/SM/SMlib.h>

/* Prototypes */

void session_init(void);
Status new_client(SmsConn connection, SmPointer data, unsigned long *maskp,
		    SmsCallbacks *callbacks, char **reasons);

#endif /* _SESSION_H */
