#ifndef RC_PRG_H
#define RC_PRG_H
#include "image.h"
typedef struct rc_prg {rc_media *image;uint16_t address;uint32_t length;unsigned p00;uint8_t name[16];} rc_prg;
/* p00 selects the 26-byte PC64 wrapper. Program bytes follow a LE load address. */
int rc_prg_open(rc_prg *p,const uint8_t *data,size_t size,int p00,char *error,size_t cap);
int rc_prg_create(rc_prg *p,uint16_t address,const uint8_t *data,size_t size,const uint8_t name[16],int p00,char *error,size_t cap);
const uint8_t *rc_prg_data(const rc_prg *p);
void rc_prg_close(rc_prg *p);
void rc_prg_save(const rc_prg *p,rc_state_writer *w);
int rc_prg_load(rc_prg *p,rc_state_reader *r);
#endif
