#ifndef RC_C64_H
#define RC_C64_H
#include "../machine_impl.h"
#include "../../hw/bus.h"
#include "../../hw/sched.h"
typedef struct rc_c64 {
    rc_hw_host host;rc_sched sched;rc_bus bus;
    rc_mos6510 cpu;rc_vic2 vic;rc_cia6526 cia[2];rc_c64kbd keyboard;rc_pla pla;rc_sid sid;
    uint8_t model,restore,port_charge,border;uint32_t hz;uint64_t tod_at;
    uint8_t rom_hashes[96],frame[520u*312u];
    rc_trace_fn trace;void *trace_ctx;
} rc_c64;
extern const rc_family rc_c64_family;
void *rc_c64_create(const rc_machine_setup *setup,char *error,size_t capacity);
rc_run_status rc_c64_run(void *self,uint64_t cycles);
#endif
