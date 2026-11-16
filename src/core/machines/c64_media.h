#ifndef RC_C64_MEDIA_H
#define RC_C64_MEDIA_H
#include "c64.h"
void rc_c64_media_init(rc_c64 *c);
void rc_c64_media_release(rc_c64 *c);
void rc_c64_media_lines(rc_c64 *c);
void rc_c64_media_tick(rc_c64 *c);
void rc_c64_iec_lines(rc_c64 *c);
int rc_c64_insert(void *self,unsigned slot,rc_media *image,char *error,size_t cap);
rc_media *rc_c64_media(const void *self,unsigned slot);
int rc_c64_eject(void *self,unsigned slot);
int rc_c64_control(void *self,unsigned slot,unsigned command,uint32_t value);
unsigned rc_c64_activity(const void *self);
unsigned rc_c64_tape_options(void *self,unsigned slot,unsigned bits);
const char *rc_c64_autoload(const void *self,unsigned slot,int *power);
struct rc_tape_deck *rc_c64_tape_deck(void *self,unsigned slot);
void rc_c64_media_save(const rc_c64 *c,rc_state_writer *w);
int rc_c64_media_load(rc_c64 *c,rc_state_reader *r);
int16_t rc_c64_tape_audio(rc_c64 *c);
#endif
