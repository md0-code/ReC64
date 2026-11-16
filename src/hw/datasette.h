/* C2N digital interface. The board supplies motor/write and routes read to
 * CIA1 FLAG; this device never calls a bus or another chip. */
#ifndef RC_DATASETTE_H
#define RC_DATASETTE_H
#include "hw.h"
#include "../core/media/tapedeck.h"
enum {RC_DATASETTE_READ};
typedef struct rc_datasette {
    rc_dev dev;rc_tape_deck deck;
    uint32_t remaining,pulse,record_cycles;
    uint8_t motor,write,read,have_record_edge,auto_play,failed;
} rc_datasette;
void rc_datasette_init(rc_datasette *s,const rc_hw_host *host);
int rc_datasette_insert(rc_datasette *s,rc_media *media,char *error,size_t cap);
void rc_datasette_eject(rc_datasette *s);
/* RC_MEDIA_PLAY/STOP/REWIND/NEXT/PREVIOUS/SEEK/RECORD. */
int rc_datasette_control(rc_datasette *s,unsigned command,uint32_t value);
void rc_datasette_auto_play(rc_datasette *s,int enabled);
void rc_datasette_motor(rc_datasette *s,int on);
void rc_datasette_write(rc_datasette *s,int high);
void rc_datasette_tick(rc_datasette *s,uint32_t cycles);
int rc_datasette_sense(const rc_datasette *s);
int rc_datasette_read(const rc_datasette *s);
int16_t rc_datasette_sample(const rc_datasette *s);
int rc_datasette_ok(const rc_datasette *s);
void rc_datasette_save(const rc_datasette *s,rc_state_writer *w);
int rc_datasette_load(rc_datasette *s,rc_state_reader *r);
#endif
