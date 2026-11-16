/* Short messages ("Tape: demo.tap"): the text and its expiry time; the status
 * line in desktop.c draws it in ReC64's own character set.
 *  */

#include "overlay.h"

#include <stdarg.h>
#include <stdio.h>

void rc_osd_show(rc_osd *o, uint32_t now_ms, const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(o->text, sizeof o->text, fmt, ap);
    va_end(ap);
    o->until_ms = now_ms + 2500;
}
