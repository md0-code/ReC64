#ifndef RC_D64_H
#define RC_D64_H
#include "image.h"
typedef struct rc_d64 {rc_media *image;unsigned tracks,sectors;} rc_d64;
typedef struct rc_d64_entry {uint8_t type,track,sector,name[16],record_length;uint16_t blocks;} rc_d64_entry;
int rc_d64_open(rc_d64 *d,const uint8_t *data,size_t size,char *error,size_t cap);
int rc_d64_create(rc_d64 *d,const uint8_t name[16],const uint8_t id[2],char *error,size_t cap);
/* New 35-track disk with one closed PRG entry; data includes its load address. */
int rc_d64_create_program(rc_d64 *d,const uint8_t name[16],const uint8_t *data,size_t size,char *error,size_t cap);
void rc_d64_close(rc_d64 *d);
unsigned rc_d64_track_sectors(unsigned track);
const uint8_t *rc_d64_sector(const rc_d64 *d,unsigned track,unsigned sector);
int rc_d64_write_sector(rc_d64 *d,unsigned track,unsigned sector,const uint8_t data[256]);
uint8_t rc_d64_error(const rc_d64 *d,unsigned track,unsigned sector);
int rc_d64_directory(const rc_d64 *d,rc_d64_entry *entries,size_t capacity,size_t *count,char *error,size_t cap);
int rc_d64_extract(const rc_d64 *d,const rc_d64_entry *entry,uint8_t **data,size_t *size,char *error,size_t cap);
void rc_d64_save(const rc_d64 *d,rc_state_writer *w);
int rc_d64_load(rc_d64 *d,rc_state_reader *r);
#endif
