#ifndef RC_PALETTE_H
#define RC_PALETTE_H
#include "state.h"
const uint32_t *rc_palette_builtin(const char *name);
const uint32_t *rc_palette_profile(const char *name,int ntsc);
void rc_palette_save(const uint32_t colors[16],rc_state_writer *w);
int rc_palette_load(uint32_t colors[16],rc_state_reader *r);
#endif
