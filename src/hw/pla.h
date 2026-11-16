/* Physical input levels; AEC=1 denotes VIC ownership as in the PLA paper.
 * Decode returns asserted chip selects (one means selected), not raw /pins. */
#ifndef RC_PLA_H
#define RC_PLA_H
#include "hw.h"
#include "../core/state.h"
enum {RC_PLA_CAS=1u<<0,RC_PLA_LORAM=1u<<1,RC_PLA_HIRAM=1u<<2,RC_PLA_CHAREN=1u<<3,RC_PLA_VA14=1u<<4,RC_PLA_A15=1u<<5,RC_PLA_A14=1u<<6,RC_PLA_A13=1u<<7,RC_PLA_A12=1u<<8,RC_PLA_BA=1u<<9,RC_PLA_AEC=1u<<10,RC_PLA_R__W=1u<<11,RC_PLA_EXROM=1u<<12,RC_PLA_GAME=1u<<13,RC_PLA_VA13=1u<<14,RC_PLA_VA12=1u<<15};
enum {RC_PLA_CASRAM=1u<<0,RC_PLA_BASIC=1u<<1,RC_PLA_KERNAL=1u<<2,RC_PLA_CHAROM=1u<<3,RC_PLA_GR_W=1u<<4,RC_PLA_I_O=1u<<5,RC_PLA_ROML=1u<<6,RC_PLA_ROMH=1u<<7};
typedef struct rc_pla {rc_dev dev;uint16_t inputs;} rc_pla;
uint8_t rc_pla_decode(uint16_t inputs);
void rc_pla_init(rc_pla *p,const rc_hw_host *host);
void rc_pla_save(const rc_pla *p,rc_state_writer *w);
int rc_pla_load(rc_pla *p,rc_state_reader *r);
#endif
