/*
 * $Id$
 *
 * ROX-Session, a very simple session manager
 * By Thomas Leonard, <tal197@users.sourceforge.net>.
 */

#ifndef _WM_H
#define _WM_H

#include <sys/types.h>

extern pid_t wm_pid;

/* Prototypes */

void start_window_manager(void);
void wm_process_died(void);

#endif /* _WM_H */
