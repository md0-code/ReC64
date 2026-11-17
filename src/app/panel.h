/* Transient status overlay; all device values are supplied by the board. */
#ifndef RC_PANEL_H
#define RC_PANEL_H
#include "../core/machine.h"

typedef struct rc_panel_status {
    unsigned drive_present,drive_device,drive_halftrack,drive_led,drive_motor;
    uint64_t tape_counter; /* Current TAP data byte offset, excluding its 20-byte header. */
    unsigned tape_motor,tape_play,tape_record;
    unsigned joystick_port; /* Active keyboard joystick: 0 off, 1 or 2. */
} rc_panel_status;

typedef struct rc_panel {
    uint8_t font[2048];
    uint8_t *pixels;
    size_t capacity;
    char lines[1][41];
    uint64_t serial;
} rc_panel;

void rc_panel_init(rc_panel *p);
void rc_panel_close(rc_panel *p);
/* Copies the guest into owned storage and overlays one 8x8 row at the
 * bottom. Palette, geometry and aspect are retained. Zero on allocation or
 * invalid-frame failure, with out still the guest; guest may equal out.
 * Call only while visible; hiding the panel simply presents the guest. */
int rc_panel_draw(rc_panel *p,const rc_frame *guest,const rc_panel_status *status,rc_frame *out);
/* The displayed row, for the headless harness. */
size_t rc_panel_text(const rc_panel *p,char *out,size_t capacity);
#endif
