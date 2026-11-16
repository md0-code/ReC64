/* MOS/Rockwell 6522. Tick once at the start of phi2, before CPU access.
 * Inputs are physical levels. IRQ callbacks use asserted=1; control outputs
 * use -1 for a released/input pin. Port outputs use 1 for released inputs. */
#ifndef RC_VIA6522_H
#define RC_VIA6522_H
#include "hw.h"
#include "../core/state.h"

enum {RC_VIA_CA1,RC_VIA_CA2,RC_VIA_CB1,RC_VIA_CB2};
enum {RC_VIA_IRQ,RC_VIA_CA2_OUT,RC_VIA_CB1_OUT,RC_VIA_CB2_OUT,
      RC_VIA_PA0=16,RC_VIA_PB0=24};
typedef struct rc_via6522 {
    rc_dev dev;
    uint64_t cycles;
    uint16_t t1,t1_latch,t2;
    uint8_t port[2],ddr[2],input[2],latched[2];
    uint8_t acr,pcr,ifr,ier,irq;
    uint8_t t1_delay,t1_reload,t1_armed,t1_pb7,t2_latch,t2_delay,t2_reload,t2_armed,pb6;
    uint8_t pins[4],control[2],pulse[2];
    uint8_t sr,sr_count,sr_active,sr_clock,sr_output,sr_pending;
} rc_via6522;
void rc_via6522_init(rc_via6522 *v,const rc_hw_host *host);
void rc_via6522_reset(rc_via6522 *v);
void rc_via6522_tick(rc_via6522 *v);
uint8_t rc_via6522_read(rc_via6522 *v,unsigned reg);
void rc_via6522_write(rc_via6522 *v,unsigned reg,uint8_t value);
void rc_via6522_input(rc_via6522 *v,int pin,int level);
void rc_via6522_port_input(rc_via6522 *v,unsigned port,uint8_t levels);
uint8_t rc_via6522_port_output(const rc_via6522 *v,unsigned port);
int rc_via6522_control_output(const rc_via6522 *v,int pin);
void rc_via6522_save(const rc_via6522 *v,rc_state_writer *w);
int rc_via6522_load(rc_via6522 *v,rc_state_reader *r);
#endif
