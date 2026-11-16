/* MOS NMOS 6502/6510. One board-granted bus cycle per clock call.
 * The board reads pins, performs the access, then supplies the read byte.
 * RDY stalls reads only. AEC is handled by the board withholding the bus.
 * Register 0/1 accesses use the port helpers on a 6510, external RAM on a
 * 6502. No CPU code calls a bus or another device. */
#ifndef RC_MOS6510_H
#define RC_MOS6510_H
#include "hw.h"
#include "../core/state.h"
enum { RC_MOS6502, RC_MOS6510 };
enum { RC_CPU_IRQ, RC_CPU_NMI, RC_CPU_RESET, RC_CPU_SO, RC_CPU_PORT0=16 };
enum { RC_P_C=1,RC_P_Z=2,RC_P_I=4,RC_P_D=8,RC_P_B=16,RC_P_U=32,RC_P_V=64,RC_P_N=128 };
typedef struct rc_mos6510 {
    rc_dev dev;
    const rc_cpu_hooks *hooks;
    uint64_t cycles;
    uint16_t pc, address, base, vector;
    uint8_t a,x,y,s,p,opcode,step,lo,value,result;
    uint8_t model,ddr,port,port_input;
    uint8_t irq,nmi,nmi_pending,irq_pending,irq_poll,resetting,interrupt,jammed,so;
    rc_cpu_cycle pins;
} rc_mos6510;
void rc_mos6510_init(rc_mos6510 *c,const rc_hw_host *host,unsigned model);
void rc_mos6510_reset(rc_mos6510 *c);
/* Begin at pc without a reset sequence (test runner and known-ROM traps). */
void rc_mos6510_start(rc_mos6510 *c);
void rc_mos6510_clock(rc_mos6510 *c,uint8_t data,int ready);
void rc_mos6510_input(rc_mos6510 *c,int pin,int level);
uint8_t rc_mos6510_port_read(const rc_mos6510 *c,unsigned reg);
void rc_mos6510_port_write(rc_mos6510 *c,unsigned reg,uint8_t value);
void rc_mos6510_port_input(rc_mos6510 *c,uint8_t pins);
void rc_mos6510_save(const rc_mos6510 *c,rc_state_writer *w);
int rc_mos6510_load(rc_mos6510 *c,rc_state_reader *r);
#endif
