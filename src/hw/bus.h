#ifndef RC_BUS_H
#define RC_BUS_H
#include "mos6510.h"
#include "vic2.h"
#include "cia6526.h"
#include "c64kbd.h"
#include "pla.h"
#include "sid.h"
typedef struct rc_bus {
    uint8_t ram[65536],colour[1024],basic[8192],kernal[8192],chargen[4096];
    rc_mos6510 *cpu;rc_vic2 *vic;rc_cia6526 *cia[2];rc_c64kbd *keyboard;rc_pla *pla;rc_sid *sid;
    uint8_t data,vic_data,game,exrom;
} rc_bus;
uint8_t rc_bus_read(rc_bus *b,uint16_t address,int side_effects);
void rc_bus_write(rc_bus *b,uint16_t address,uint8_t value);
uint8_t rc_bus_vic(rc_bus *b,uint16_t address);
void rc_bus_keyboard(rc_bus *b);
void rc_bus_save(const rc_bus *b,rc_state_writer *w);
int rc_bus_load(rc_bus *b,rc_state_reader *r);
#endif
