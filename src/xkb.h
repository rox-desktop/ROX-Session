#ifndef _XKB_H
#define _XKB_H

void set_xkb_layout(const char *command);

void set_xkb_repeat(gboolean repeat, int delay, int interval);
void set_xkb_numlock(const gboolean keystate);
void set_xkb_capslock(const gboolean keystate);
void set_xkb_scrolllock(const gboolean keystate);

#endif /* _XKB_H */
