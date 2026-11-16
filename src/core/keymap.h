/* Symbolic text and configurable physical bindings. Host ids are opaque. */
#ifndef RC_KEYMAP_H
#define RC_KEYMAP_H
#include "machine.h"
#define RC_KEYMAP_HOSTS 512
#define RC_KEYMAP_QUEUE 256
typedef struct rc_keymap {
    rc_machine *machine;const rc_config *config;
    uint8_t keys[RC_KEYMAP_HOSTS][2],held[256],sent[256];
    rc_typing_desc queue[RC_KEYMAP_QUEUE];unsigned read,count,phase;
} rc_keymap;
void rc_keymap_init(rc_keymap *k,rc_machine *machine,const rc_config *config);
void rc_keymap_clear(rc_keymap *k);
/* A binding consumes its host event; a release uses the original press. */
int rc_keymap_key(rc_keymap *k,unsigned host,const char *name,int down);
/* UTF-8, translated using the family's typing table; 0: unsupported or full. */
int rc_keymap_text(rc_keymap *k,const char *text);
void rc_keymap_frame(rc_keymap *k);
int rc_keymap_action(const rc_family *f,const char *name,uint8_t keys[2]);
#endif
