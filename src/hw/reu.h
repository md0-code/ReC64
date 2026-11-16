/* Original 8726 register/DMA interface; the board owns all bus arbitration. */
#ifndef RC_REU_H
#define RC_REU_H
#include "../core/state.h"

typedef struct rc_reu_pins {
    uint16_t address;
    uint8_t data,write,dma,valid;
} rc_reu_pins;
typedef struct rc_reu {
    uint8_t *ram;
    uint32_t size;
    uint64_t cycles;
    uint8_t reg[11],shadow[7],active,phase,swap_byte,data_latch;
} rc_reu;
/* Zero bytes disables the device. Otherwise powers of two, 128 KiB..16 MiB.
 * init starts a new object; free before replacing it. Reset retains RAM. */
int rc_reu_init(rc_reu *r,uint32_t bytes);
void rc_reu_free(rc_reu *r);
void rc_reu_reset(rc_reu *r);
uint8_t rc_reu_drive_mask(const rc_reu *r,uint16_t address);
uint8_t rc_reu_read(rc_reu *r,uint16_t address,uint8_t bus,int side_effects);
void rc_reu_write(rc_reu *r,uint16_t address,uint8_t value);
/* Observe granted CPU writes, including when I/O is banked out. */
void rc_reu_observe_write(rc_reu *r,uint16_t address);
rc_reu_pins rc_reu_bus(const rc_reu *r);
/* Call once after each phi2. BA is its physical level (1 permits DMA).
 * accepted=1 only after the board has drained pending CPU writes, owns AEC,
 * and has performed this pins access. Stalls preserve the transfer pins.
 * CPU is stopped while dma=1. No invented startup/termination wait cycles.
 * DMA accesses use mapped memory but bypass the 6510's internal port 0/1.
 * Store/fetch/verify: 1 accepted cycle/byte. Swap: read then write, 2/byte. */
void rc_reu_tick(rc_reu *r,int ba,int accepted,uint8_t read_data);
int rc_reu_irq(const rc_reu *r);
void rc_reu_save(const rc_reu *r,rc_state_writer *w);
int rc_reu_load(rc_reu *r,rc_state_reader *s);
#endif
