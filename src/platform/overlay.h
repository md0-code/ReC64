/* Short messages: the text and how long it stays on the status line. */

#ifndef RC_OVERLAY_H
#define RC_OVERLAY_H

#include <stdint.h>

typedef struct rc_osd {
    char text[128];
    uint32_t until_ms;
} rc_osd;

/* Shows a message for a couple of seconds from now_ms. */
void rc_osd_show(rc_osd *o, uint32_t now_ms, const char *fmt, ...);

#endif /* RC_OVERLAY_H */
