/*
 * $Id$
 *
 * ROX-Session, a very simple session manager
 * By Thomas Leonard, <tal197@users.sourceforge.net>.
 */

#include <glib.h>
#include <libxml/tree.h>

/* Each option has a static Option structure. This is initialised by
 * calling option_add_int() or similar. See options.c for details.
 * This structure is read-only.
 */
typedef struct _Option Option;
