#include "sched.h"
#include <string.h>
static void recompute(rc_sched *s) {
    s->next = RC_NEVER;
    for (unsigned i = 0; i < s->count; ++i) if (s->when[i] < s->next) s->next = s->when[i];
}
void rc_sched_init(rc_sched *s) { memset(s, 0, sizeof *s); s->next = RC_NEVER; }
int rc_sched_set(rc_sched *s, rc_dev *dev, uint64_t when) {
    unsigned i = 0;
    if (!dev || !dev->ops || !dev->ops->event) return 0;
    while (i < s->count && s->dev[i] != dev) ++i;
    if (i == s->count) {
        if (when == RC_NEVER) return 1;
        if (s->count == RC_SCHED_MAX) return 0;
        s->dev[s->count++] = dev;
    }
    s->when[i] = when < s->now ? s->now : when;
    recompute(s); return 1;
}
uint64_t rc_sched_get(const rc_sched *s, const rc_dev *dev) {
    for (unsigned i = 0; i < s->count; ++i) if (s->dev[i] == dev) return s->when[i];
    return RC_NEVER;
}
void rc_sched_run(rc_sched *s, uint64_t until) {
    if (s->in_event || until < s->now) return;
    while (rc_sched_due(s, until)) {
        unsigned first = 0;
        for (unsigned i = 1; i < s->count; ++i) if (s->when[i] < s->when[first]) first = i;
        s->now = s->when[first]; rc_dev *dev = s->dev[first];
        s->when[first] = RC_NEVER; recompute(s); s->in_event = 1;
        dev->ops->event(dev, s->now); s->in_event = 0;
    }
    s->now = until;
}
int rc_sched_cycles(rc_sched *s, const rc_cycle_host *b, uint64_t cycles) {
    if (s->in_event || cycles >= RC_NEVER - s->now) return 0;
    const uint64_t until = s->now + cycles;
    while (s->now < until) {
        rc_sched_run(s, s->now); const uint64_t t = s->now;
        if (b->phi1) b->phi1(b->ctx, t);
        if (b->phi2) b->phi2(b->ctx, t);
        if (b->tick) b->tick(b->ctx, t);
        ++s->now;
    }
    rc_sched_run(s, until); return 1;
}
