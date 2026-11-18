/* The 1541's sounds, played from recordings of the drive. The board supplies
 * mechanism status and requests one sample at each 48 kHz audio boundary,
 * including while muted. */
#ifndef RC_DRIVESND_H
#define RC_DRIVESND_H
#include "hw.h"
#include "../core/state.h"

#define RC_DRIVESND_RATE 48000u
#define RC_DRIVESND_VOICES 8u
/* A motor recording being played: which, where in it and how loud (256 is full). */
typedef struct rc_drivesnd_source {
    uint32_t at;
    uint16_t gain;
    uint8_t kind;
} rc_drivesnd_source;
/* A head sound being played. */
typedef struct rc_drivesnd_voice {
    uint32_t age;
    uint8_t kind;
} rc_drivesnd_voice;
typedef struct rc_drivesnd {
    rc_dev dev;
    uint64_t steps, bumps;
    rc_drivesnd_source now, before;     /* the motor, and what it fades out of */
    rc_drivesnd_voice voice[RC_DRIVESND_VOICES];
    uint32_t fade, step_gap, knock_gap;
    int32_t line[4];                    /* the last samples at the recordings' rate */
    uint8_t motor, next_voice, step_turn, knock_phase, half;
} rc_drivesnd;

void rc_drivesnd_init(rc_drivesnd *s);
void rc_drivesnd_reset(rc_drivesnd *s);
/* Monotonic physical-event counters; a counter reset rebases that counter.
 * Duplicate status has no effect. Work per call is bounded to eight voices. */
void rc_drivesnd_status(rc_drivesnd *s,int motor,uint64_t steps,uint64_t bumps);
/* Advances exactly one sample; no sample queue or host time. */
int16_t rc_drivesnd_sample(rc_drivesnd *s);
void rc_drivesnd_save(const rc_drivesnd *s,rc_state_writer *w);
int rc_drivesnd_load(rc_drivesnd *s,rc_state_reader *r);
#endif
