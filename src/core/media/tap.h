#ifndef RC_TAP_H
#define RC_TAP_H
#include "image.h"
typedef struct rc_tap {rc_media *image;size_t position;uint8_t version;} rc_tap;
int rc_tap_open(rc_tap *t,const uint8_t *data,size_t size,char *error,size_t cap);
int rc_tap_create(rc_tap *t,unsigned version,char *error,size_t cap);
int rc_tap_next(rc_tap *t,uint32_t *cycles);
void rc_tap_rewind(rc_tap *t);
int rc_tap_append(rc_tap *t,uint32_t cycles,char *error,size_t cap);
void rc_tap_close(rc_tap *t);
void rc_tap_save(const rc_tap *t,rc_state_writer *w);
int rc_tap_load(rc_tap *t,rc_state_reader *r);
#endif
