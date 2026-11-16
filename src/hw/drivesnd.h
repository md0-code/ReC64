/* Original 1541 sound synthesis. The board supplies mechanism status and
 * requests one sample at each 48 kHz audio boundary, including while muted. */
#ifndef RC_DRIVESND_H
#define RC_DRIVESND_H
#include "hw.h"
#include "../core/state.h"

#define RC_DRIVESND_RATE 48000u
#define RC_DRIVESND_VOICES 8u
typedef struct rc_drivesnd_voice {
    uint32_t phase[3], envelope, remaining;
    uint8_t kind;
} rc_drivesnd_voice;
typedef struct rc_drivesnd {
    rc_dev dev;
    uint64_t steps, bumps;
    uint32_t phase[3], random, age, gain, start_gain;
    int32_t noise;
    rc_drivesnd_voice voice[RC_DRIVESND_VOICES];
    uint8_t motor, mode, next_voice;
} rc_drivesnd;

void rc_drivesnd_init(rc_drivesnd *s);
void rc_drivesnd_reset(rc_drivesnd *s);
/* Monotonic physical-event counters; a counter reset rebases that counter.
 * Duplicate status has no effect. Work per call is bounded to eight voices. */
void rc_drivesnd_status(rc_drivesnd *s,int motor,uint64_t steps,uint64_t bumps);
/* Advances exactly one sample; no sample queue, floating point or host time. */
int16_t rc_drivesnd_sample(rc_drivesnd *s);
void rc_drivesnd_save(const rc_drivesnd *s,rc_state_writer *w);
int rc_drivesnd_load(rc_drivesnd *s,rc_state_reader *r);
#endif
