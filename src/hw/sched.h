/* Guest time is CPU cycles. A board resolves phi1/phi2 bus ownership and
 * BA/AEC/RDY, then ticks the chips. Deadlines run before phi1, in time order;
 * ties keep registration order. Chip loads restore their own deadlines. */
#ifndef RC_SCHED_H
#define RC_SCHED_H
#include "hw.h"
#define RC_SCHED_MAX 32
typedef struct rc_sched {
    rc_dev *dev[RC_SCHED_MAX];
    uint64_t when[RC_SCHED_MAX];
    unsigned count;
    uint64_t next, now;
    int in_event;
} rc_sched;
typedef struct rc_cycle_host {
    void *ctx;
    void (*phi1)(void *ctx, uint64_t cycle);
    void (*phi2)(void *ctx, uint64_t cycle);
    void (*tick)(void *ctx, uint64_t cycle);
} rc_cycle_host;
void rc_sched_init(rc_sched *s);
int rc_sched_set(rc_sched *s, rc_dev *dev, uint64_t when);
uint64_t rc_sched_get(const rc_sched *s, const rc_dev *dev);
void rc_sched_run(rc_sched *s, uint64_t until);
int rc_sched_cycles(rc_sched *s, const rc_cycle_host *board, uint64_t cycles);
static inline int rc_sched_due(const rc_sched *s, uint64_t t) { return s->next != RC_NEVER && s->next <= t; }
#endif
