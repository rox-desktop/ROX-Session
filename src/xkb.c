#include <string.h>
#include <stdio.h>
#include <glib.h>
#include "xkb.h"

static gchar **xkb_layout = NULL;	

/* Used by settings.c */
void set_xkb_layout(const char *command)
{
	GError	*error = NULL;
	gint	pid;
	char  **argv = NULL;
	gchar cmd[128];
	gchar *pos;
	if (!*command)
		command = NULL;
	if (strlen(command) > 60)
			return; /* command is fishy, beware buffer overflows */
	g_strfreev(xkb_layout);
	xkb_layout = g_strsplit(command, ";", 0);
	pos = g_stpcpy (cmd, "/usr/X11R6/bin/setxkbmap");
	if (!xkb_layout[0] || strlen(xkb_layout[0]) == 0)
			return;
	if (!xkb_layout[1] || strlen(xkb_layout[1]) == 0)
			return;
	pos = g_stpcpy (pos, " -layout ");
	pos = g_stpcpy (pos, xkb_layout[1]);
	if (!xkb_layout[2] || strlen(xkb_layout[2]) == 0)
			return;
	pos = g_stpcpy (pos, " -model ");
	pos = g_stpcpy (pos, xkb_layout[2]);
	if (xkb_layout[3]) {
		if (strlen(xkb_layout[3]) > 0) {
			pos = g_stpcpy (pos, " -variant ");
			pos = g_stpcpy (pos, xkb_layout[3]);
		}
		if (xkb_layout[4]) {
			if (strlen(xkb_layout[4]) > 0) {
				pos = g_stpcpy (pos, " -options ");
				pos = g_stpcpy (pos, xkb_layout[4]);
			}
			if (xkb_layout[5] && strlen(xkb_layout[5]) > 0) {
				pos = g_stpcpy (pos, " -rules ");
				pos = g_stpcpy (pos, xkb_layout[5]);
			}
		}
	}

	fprintf(stderr, cmd);

	if (g_shell_parse_argv(cmd, NULL, &argv, &error))
		g_spawn_async(NULL, argv, NULL,
			G_SPAWN_SEARCH_PATH | G_SPAWN_DO_NOT_REAP_CHILD |
			G_SPAWN_STDOUT_TO_DEV_NULL,
			NULL, NULL, &pid, &error);
	g_strfreev(argv);
}
