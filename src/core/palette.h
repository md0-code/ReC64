#ifndef RC_PALETTE_H
#define RC_PALETTE_H
#include "state.h"
/* The colours of `default` or `color-corrected`; NULL for another name. */
const uint32_t *rc_palette_builtin(const char *name);
void rc_palette_save(const uint32_t colors[16],rc_state_writer *w);
int rc_palette_load(uint32_t colors[16],rc_state_reader *r);
#endif
