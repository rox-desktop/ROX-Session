/*
 * $Id$
 *
 * ROX-Session, a very simple session manager
 * By Thomas Leonard, <tal197@users.sourceforge.net>.
 */

#ifndef _MAIN_H
#define _MAIN_H

extern int number_of_windows;
extern guchar *app_dir;

/* Prototypes */
int main(int argc, char **argv);
void login_failure(int error);

#endif /* _MAIN_H */
