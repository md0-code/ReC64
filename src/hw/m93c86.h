/* ST M93C86 MICROWIRE EEPROM, ORG tied high: 1024 16-bit words. */
#ifndef RC_M93C86_H
#define RC_M93C86_H
#include "../core/state.h"
#define RC_M93C86_SIZE 2048u
typedef struct rc_m93c86 {
    uint8_t memory[RC_M93C86_SIZE]; /* high byte first in a raw EEPROM image */
    uint32_t clock_hz,shift;
    uint64_t remaining;
    uint16_t address,data;
    uint8_t cs,clock,bits,phase,pending,operation,enabled,q,ready;
} rc_m93c86;
void rc_m93c86_init(rc_m93c86 *e,uint32_t clock_hz);
void rc_m93c86_pins(rc_m93c86 *e,int cs,int clock,int data);
void rc_m93c86_tick(rc_m93c86 *e,uint64_t cycles);
/* q is 0/1, or 2 when the output is high impedance. */
void rc_m93c86_save(const rc_m93c86 *e,rc_state_writer *w);
int rc_m93c86_load(rc_m93c86 *e,rc_state_reader *r);
#endif
