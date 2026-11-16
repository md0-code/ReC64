/* HVSC PSID v1-v4 / RSID v2-v4 containers. The bytes remain caller-owned. */
#ifndef RC_SIDFILE_H
#define RC_SIDFILE_H
#include <stddef.h>
#include <stdint.h>

enum { RC_SID_UNKNOWN, RC_SID_PAL, RC_SID_NTSC, RC_SID_BOTH };
enum { RC_SID_MODEL_UNKNOWN, RC_SID_MODEL_6581, RC_SID_MODEL_8580, RC_SID_MODEL_BOTH };
typedef struct rc_sidfile {
    const uint8_t *payload;
    uint32_t length,speed;
    uint16_t version,load,init,play,songs,start_song,flags;
    uint8_t rsid,basic,clock,model,start_page,pages;
    char title[33],author[33],released[33];
} rc_sidfile;
typedef struct rc_sid_driver {
    uint8_t bytes[256];
    uint16_t address,entry,irq,indirect_irq,nmi,idle;
    unsigned length;
} rc_sid_driver;
/* Failure leaves *out unchanged. Song numbers in the header are one-based. */
int rc_sidfile_parse(rc_sidfile *out,const uint8_t *data,size_t size,char *error,size_t cap);
int rc_sidfile_cia(const rc_sidfile *s,unsigned song);
uint8_t rc_sidfile_bank(uint16_t address);
/* Original 6510 driver, one reserved page. song is zero-based. */
int rc_sidfile_driver(const rc_sidfile *s,unsigned song,rc_sid_driver *driver,char *error,size_t cap);
#endif
