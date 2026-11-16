/* EasyFlash 1 hardware board: two flash chips and always-visible I/O2 RAM. */
#ifndef RC_EASYFLASH_H
#define RC_EASYFLASH_H
#include "cart.h"
#include "am29f040.h"
typedef struct rc_easyflash {
    rc_cart *port;
    uint8_t *storage;
    rc_am29f040 flash[2];
    uint8_t ram[256],bank,control,boot,ba;
    int sync_failed;
} rc_easyflash;
void rc_easyflash_init(rc_easyflash *e);
/* Initialized object and parsed hardware-32 CRT required. Binds port ops. */
int rc_easyflash_attach(rc_easyflash *e,rc_cart *port,uint32_t clock_hz,char *error,size_t cap);
void rc_easyflash_close(rc_easyflash *e);
/* Copy completed writes to CRT packets, adding omitted banks as necessary.
 * The host owns file I/O. Tick invokes sync after embedded operations finish. */
int rc_easyflash_sync(rc_easyflash *e);
void rc_easyflash_boot(rc_easyflash *e,int enabled);
void rc_easyflash_save(const rc_easyflash *e,rc_state_writer *w);
int rc_easyflash_load(rc_easyflash *e,rc_state_reader *r);
#endif
