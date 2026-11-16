/* AMD Am29F040B: 512 KiB, eight uniform 64 KiB sectors. */
#ifndef RC_AM29F040_H
#define RC_AM29F040_H
#include "../core/state.h"
#define RC_AM29F040_SIZE 0x80000u
typedef struct rc_am29f040 {
    uint8_t *memory; /* caller owned, stable 512 KiB array */
    uint32_t clock_hz,address;
    uint64_t remaining,erase_remaining,timeout,dirty_banks;
    uint8_t command,autoselect,operation,sectors,suspended,toggle,data,failed;
} rc_am29f040;
void rc_am29f040_init(rc_am29f040 *f,uint8_t *memory,uint32_t clock_hz);
/* Read-array reset command: ongoing embedded operations ignore it. */
void rc_am29f040_reset(rc_am29f040 *f);
uint8_t rc_am29f040_read(rc_am29f040 *f,uint32_t address,int side_effects);
void rc_am29f040_write(rc_am29f040 *f,uint32_t address,uint8_t value);
void rc_am29f040_tick(rc_am29f040 *f,uint64_t cycles);
void rc_am29f040_save(const rc_am29f040 *f,rc_state_writer *w);
int rc_am29f040_load(rc_am29f040 *f,rc_state_reader *r);
#endif
