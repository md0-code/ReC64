/* Host-speed policy only: all guest CPU and device cycles still execute. */
#ifndef RC_FASTLOAD_H
#define RC_FASTLOAD_H
#include "../core/state.h"
typedef struct rc_fastload {
    uint32_t clock_hz;
    uint64_t now,last_edges,last_edge_at;
    uint8_t motor,recent_edge;
} rc_fastload;
/* clock_hz is the nonzero guest clock. init starts at cycle/edge count zero. */
void rc_fastload_init(rc_fastload *s,uint32_t clock_hz);
/* Seed the current timeline and IEC counter without inventing bus activity. */
void rc_fastload_reset(rc_fastload *s,uint64_t now,uint64_t edge_count);
/* Observe each guest cycle, after resolving IEC and actual drive motor pins.
 * Serial activity expires after ceil(clock_hz/50) cycles without a new edge:
 * 20 ms bridges a frame poll, while a line held low eventually becomes idle.
 * Stopping the motor adds no tail of its own. The shell owns the enable flag. */
void rc_fastload_observe(rc_fastload *s,uint64_t now,int motor,uint64_t edge_count);
int rc_fastload_active(const rc_fastload *s);
void rc_fastload_save(const rc_fastload *s,rc_state_writer *w);
int rc_fastload_load(rc_fastload *s,rc_state_reader *r);
#endif
