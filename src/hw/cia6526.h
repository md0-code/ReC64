/* MOS 6526/8521 CIA. Tick at the start of phi2, then perform the CPU access.
 * CNT/SP/FLAG are physical levels; host IRQ is asserted=1. The board supplies
 * port input levels and connects port outputs; no CIA calls another chip. */
#ifndef RC_CIA6526_H
#define RC_CIA6526_H
#include "hw.h"
#include "../core/state.h"
enum {RC_CIA_6526,RC_CIA_8521};
enum {RC_CIA_IRQ,RC_CIA_CNT_OUT,RC_CIA_SP_OUT,RC_CIA_PC_OUT,RC_CIA_PA0=16,RC_CIA_PB0=24};
enum {RC_CIA_CNT,RC_CIA_SP,RC_CIA_FLAG,RC_CIA_TOD};
typedef struct rc_cia_timer {
    uint16_t counter,latch;
    uint8_t control,count_pipe,load_pipe,force,oneshot,toggle,pulse;
} rc_cia_timer;
typedef struct rc_cia6526 {
    rc_dev dev;
    rc_cia_timer timer[2];
    uint64_t cycles;
    uint8_t model,port[2],ddr[2],input[2],icr,mask,irq,irq_delay;
    uint8_t cnt,sp,flag,cnt_edge,tod_pin,pc;
    uint8_t sdr,shift,serial_bits,serial_active,serial_pending,cnt_out,sp_out;
    uint8_t tod[4],alarm[4],tod_latch[4],tod_latched,tod_stopped,tod_divider,alarm_match;
    uint8_t announced,pins[2];          /* the port levels last sent to the host; never saved */
} rc_cia6526;
void rc_cia6526_init(rc_cia6526 *c,const rc_hw_host *host,unsigned model);
void rc_cia6526_reset(rc_cia6526 *c);
void rc_cia6526_tick(rc_cia6526 *c);
uint8_t rc_cia6526_read(rc_cia6526 *c,unsigned reg);
void rc_cia6526_write(rc_cia6526 *c,unsigned reg,uint8_t value);
void rc_cia6526_input(rc_cia6526 *c,int pin,int level);
void rc_cia6526_port_input(rc_cia6526 *c,unsigned port,uint8_t pins);
uint8_t rc_cia6526_port_output(const rc_cia6526 *c,unsigned port);
void rc_cia6526_tod_pulse(rc_cia6526 *c);
void rc_cia6526_save(const rc_cia6526 *c,rc_state_writer *w);
int rc_cia6526_load(rc_cia6526 *c,rc_state_reader *r);
#endif
