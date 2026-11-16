/* Host-independent C64 keyboard overlay and multi-pointer contact ownership. */
#ifndef RC_OSK_H
#define RC_OSK_H
#include "../core/machines/c64_layout.h"
#define RC_OSK_POINTERS 16
typedef void (*rc_osk_set_key)(void *ctx,unsigned key,int down);
typedef struct rc_osk_contact {int64_t id;int cap;uint8_t used;} rc_osk_contact;
typedef struct rc_osk {
    rc_osk_set_key set_key;void *ctx;
    uint8_t font[2048],held[65],sent[65],sticky[65];
    rc_osk_contact contacts[RC_OSK_POINTERS];
    uint8_t *pixels;size_t capacity;uint64_t serial;
    int visible,selected,accepted,left,top,width,height;
} rc_osk;
void rc_osk_init(rc_osk *o,rc_osk_set_key set_key,void *ctx);
/* Rebind after accepting a restored machine; reapplies sticky contacts. */
void rc_osk_bind(rc_osk *o,rc_osk_set_key set_key,void *ctx);
void rc_osk_open(rc_osk *o,int visible);
/* Clears every contact and sticky modifier, also on focus loss. */
void rc_osk_release(rc_osk *o);
void rc_osk_close(rc_osk *o);
/* Frame coordinates, as returned by draw; down: 1 press/move, 0 release,
 * -1 cancel (cancel never latches a modifier). Independent pointer IDs. */
int rc_osk_pointer(rc_osk *o,int64_t id,int x,int y,int down);
/* UP/DOWN/LEFT/RIGHT navigate; A/RETURN/SPACE press/release the selection;
 * B/ESCAPE close. These are UI actions, not symbolic guest typing. */
int rc_osk_key(rc_osk *o,const char *name,int down);
int rc_osk_draw(rc_osk *o,const rc_frame *guest,unsigned bottom_margin,rc_frame *out);
size_t rc_osk_text(const rc_osk *o,char *out,size_t capacity);
/* Payload only, in the caller's current chunk. Sticky modifiers, selection
 * and visibility persist; contacts and accept-held state do not. */
void rc_osk_save(const rc_osk *o,rc_state_writer *w);
int rc_osk_load(rc_osk *o,rc_state_reader *r);
#endif
