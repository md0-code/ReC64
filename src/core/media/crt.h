/* Peter Schepers CRT format: descriptors point into the original file. */
#ifndef RC_CRT_H
#define RC_CRT_H
#include "../media.h"
#include "../state.h"
enum { RC_CRT_NORMAL=0, RC_CRT_ACTION_REPLAY=1, RC_CRT_KCS=2, RC_CRT_FINAL_III=3,
    RC_CRT_SIMONS_BASIC=4, RC_CRT_OCEAN=5, RC_CRT_FUN_PLAY=7,
    RC_CRT_SUPER_GAMES=8, RC_CRT_EPYX=10, RC_CRT_FINAL_I=13, RC_CRT_SYSTEM3=15, RC_CRT_DINAMIC=17,
    RC_CRT_ZAXXON=18, RC_CRT_MAGIC_DESK=19, RC_CRT_RETRO_REPLAY=36,
    RC_CRT_SUPER_SNAPSHOT5=20, RC_CRT_ACTION_REPLAY4=30, RC_CRT_EASYFLASH=32 };
typedef struct rc_crt_chip {
    uint16_t type,bank,address,size;
    size_t offset; /* first data byte in image; not the CHIP header */
} rc_crt_chip;
typedef struct rc_crt {
    rc_media *image; /* borrowed with attach, owned with open/load */
    rc_crt_chip *chips;
    size_t count;
    uint16_t version,hardware;
    uint8_t game,exrom,owned;
    char name[33];
} rc_crt;
void rc_crt_init(rc_crt *c);
/* All constructors require an initialized object. Failure leaves it unchanged. */
int rc_crt_attach(rc_crt *c,rc_media *image,char *error,size_t cap);
int rc_crt_open(rc_crt *c,const uint8_t *data,size_t size,char *error,size_t cap);
void rc_crt_close(rc_crt *c);
const rc_crt_chip *rc_crt_find(const rc_crt *c,unsigned bank,unsigned address);
int rc_crt_write(rc_crt *c,const rc_crt_chip *chip,unsigned offset,uint8_t value);
void rc_crt_save(const rc_crt *c,rc_state_writer *w);
/* A borrowed image stays borrowed and retains its pointer on restore. */
int rc_crt_load(rc_crt *c,rc_state_reader *r);
#endif
