/*
 * $Id$
 *
 * ROX-Session, a very simple session manager
 * By Thomas Leonard, <tal197@users.sourceforge.net>.
 */

#ifndef _SETTINGS_H
#define _SETTINGS_H

#define ROX_SESSION_ERROR "net.sf.rox.Session.Error"

#include "xsettings-manager.h"

extern XSettingsManager *xsettings_manager;

/* Prototypes */
void settings_init(void);

#endif /* _SETTINGS_H */
