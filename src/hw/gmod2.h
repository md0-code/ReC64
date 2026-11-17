/* Individual Computers GMod2: 29F040 flash and x16 M93C86 EEPROM. */
#ifndef RC_GMOD2_H
#define RC_GMOD2_H
#include "cart.h"
#include "am29f040.h"
#include "m93c86.h"
typedef struct rc_gmod2 {
    rc_cart *port;
    uint8_t *storage;
    rc_am29f040 flash;
    rc_m93c86 eeprom;
    uint8_t control,ba,write_cycle;
} rc_gmod2;
void rc_gmod2_init(rc_gmod2 *g);
void rc_gmod2_close(rc_gmod2 *g);
int rc_gmod2_attach(rc_gmod2 *g,rc_cart *port,uint32_t clock_hz,char *error,size_t cap);
/* Copy completed flash changes to CRT packets. EEPROM file I/O is board owned. */
int rc_gmod2_sync(rc_gmod2 *g);
#endif
