#ifndef RC_C64_EXPANSION_H
#define RC_C64_EXPANSION_H
#include "c64.h"
int rc_c64_expansion_init(rc_c64 *c,const rc_machine_setup *setup,char *error,size_t cap);
void rc_c64_expansion_release(rc_c64 *c);
void rc_c64_expansion_reset(rc_c64 *c);
void rc_c64_expansion_lines(rc_c64 *c);
void rc_c64_expansion_tick(rc_c64 *c);
int rc_c64_cart_insert(rc_c64 *c,rc_media *image,char *error,size_t cap);
int rc_c64_cart_flush(rc_c64 *c);
int rc_c64_cart_freeze(rc_c64 *c,uint32_t value);
void rc_c64_expansion_save(const rc_c64 *c,rc_state_writer *w);
int rc_c64_expansion_load(rc_c64 *c,rc_state_reader *r);
#endif
