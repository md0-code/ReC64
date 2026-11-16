#ifndef RC_T64_H
#define RC_T64_H
#include "image.h"
typedef struct rc_t64 {rc_media *image;unsigned capacity,count;} rc_t64;
typedef struct rc_t64_entry {uint16_t address,slot;uint32_t offset,length;uint8_t name[16],type;} rc_t64_entry;
typedef struct rc_t64_program {uint16_t address;const uint8_t *data;size_t size;uint8_t name[16];} rc_t64_program;
int rc_t64_open(rc_t64 *t,const uint8_t *data,size_t size,char *error,size_t cap);
int rc_t64_entry_at(const rc_t64 *t,unsigned entry,rc_t64_entry *out);
int rc_t64_create(rc_t64 *t,const rc_t64_program *programs,unsigned count,const uint8_t name[24],char *error,size_t cap);
void rc_t64_close(rc_t64 *t);
void rc_t64_save(const rc_t64 *t,rc_state_writer *w);
int rc_t64_load(rc_t64 *t,rc_state_reader *r);
#endif
