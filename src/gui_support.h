/*
 * $Id$
 *
 * ROX-Session, a very simple session manager
 * By Thomas Leonard, <tal197@users.sourceforge.net>.
 */

#ifndef _GUI_SUPPORT_H
#define _GUI_SUPPORT_H

#include <gtk/gtk.h>
#include <libxml/parser.h>

GtkWidget *button_new_mixed(const char *stock, const char *message);
void report_error(const char *message, ...);
int get_choice(const char *message, int number_of_buttons, ...);
int save_xml_file(xmlDocPtr doc, gchar *filename);

#endif /* _GUI_SUPPORT_H */
