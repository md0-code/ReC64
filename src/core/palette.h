#ifndef RC_PALETTE_H
#define RC_PALETTE_H
#include "state.h"
const uint32_t *rc_palette_builtin(const char *name);
int rc_palette_parse(const uint8_t *bytes,size_t size,uint32_t colors[16],char *error,size_t capacity);
size_t rc_palette_write(const uint32_t colors[16],char *out,size_t capacity);
void rc_palette_save(const uint32_t colors[16],rc_state_writer *w);
int rc_palette_load(uint32_t colors[16],rc_state_reader *r);
#endif
