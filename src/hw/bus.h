#ifndef RC_BUS_H
#define RC_BUS_H
#include "mos6510.h"
#include "vic2.h"
#include "cia6526.h"
#include "c64kbd.h"
#include "pla.h"
#include "sid.h"
#include "mouse1351.h"
typedef struct rc_bus_expansion {
    void *ctx;
    void (*observe)(void *ctx,uint16_t address,int write,int effects,int vic);
    uint8_t (*read)(void *ctx,uint16_t address,unsigned selected,uint8_t data,int driven,int effects,int vic);
    void (*write)(void *ctx,uint16_t address,unsigned selected,uint8_t value);
} rc_bus_expansion;
typedef struct rc_bus {
    uint8_t ram[65536],colour[1024],basic[8192],kernal[8192],chargen[4096];
    rc_mos6510 *cpu;rc_vic2 *vic;rc_cia6526 *cia[2];rc_c64kbd *keyboard;rc_pla *pla;rc_sid *sid;
    uint8_t data,vic_data,game,exrom;
    rc_bus_expansion expansion;uint8_t dma;
    rc_mouse1351 mouse[2];uint8_t control_type[2],paddle[2][2];
    uint16_t pot_phase;uint8_t pot_value[2],pot_charge[2];
} rc_bus;
uint8_t rc_bus_read(rc_bus *b,uint16_t address,int side_effects);
void rc_bus_write(rc_bus *b,uint16_t address,uint8_t value);
uint8_t rc_bus_vic(rc_bus *b,uint16_t address);
uint8_t rc_bus_dma_read(rc_bus *b,uint16_t address);
void rc_bus_dma_write(rc_bus *b,uint16_t address,uint8_t value);
void rc_bus_keyboard(rc_bus *b);
void rc_bus_pot_tick(rc_bus *b);
void rc_bus_save(const rc_bus *b,rc_state_writer *w);
int rc_bus_load(rc_bus *b,rc_state_reader *r);
#endif
