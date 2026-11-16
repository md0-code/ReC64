/* Original KERNAL tape blocks carried by a TAP pulse stream. */
#ifndef RC_TAPEDECK_H
#define RC_TAPEDECK_H
#include "tap.h"
typedef struct rc_tape_entry {
    uint8_t name[16];uint16_t address;uint32_t length;
    size_t offset,end_offset;
} rc_tape_entry;
int rc_tape_directory(const rc_tap *t,rc_tape_entry *entries,size_t capacity,size_t *count,char *error,size_t cap);
int rc_tape_extract(const rc_tap *t,unsigned ordinal,uint16_t *address,uint8_t **data,size_t *size,char *error,size_t cap);
int rc_tape_encode(rc_tap *out,uint16_t address,const uint8_t *data,size_t size,const uint8_t name[16],char *error,size_t cap);

/* The transport borrows mounted media; a state loaded without mounted media
 * owns its restored copy. Detach never closes a borrowed image. */
typedef struct rc_tape_deck {
    rc_tap tape;uint8_t owned,play,record,eof;
} rc_tape_deck;
void rc_tape_deck_init(rc_tape_deck *d);
int rc_tape_deck_insert(rc_tape_deck *d,rc_media *media,char *error,size_t cap);
void rc_tape_deck_eject(rc_tape_deck *d);
int rc_tape_deck_seek(rc_tape_deck *d,unsigned ordinal);
int rc_tape_deck_next(rc_tape_deck *d,int direction);
void rc_tape_deck_save(const rc_tape_deck *d,rc_state_writer *w);
int rc_tape_deck_load(rc_tape_deck *d,rc_state_reader *r);
#endif
