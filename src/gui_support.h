/*
 * $Id$
 *
 * ROX-Session, a very simple session manager
 * By Thomas Leonard, <tal197@users.sourceforge.net>.
 */

#ifndef _GUI_SUPPORT_H
#define _GUI_SUPPORT_H

#include <gtk/gtk.h>

GtkWidget *button_new_mixed(const char *stock, const char *message);
void report_error(const char *message, ...);

#endif /* _GUI_SUPPORT_H */
