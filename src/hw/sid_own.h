/* Original SID candidate; hardware sources are pinned in docs/sources.lock.json. */
#ifndef RC_SID_OWN_H
#define RC_SID_OWN_H
#include <stddef.h>
#include <stdint.h>
#include "../core/state.h"
#ifdef __cplusplus
extern "C" {
#endif
#define RC_SID_OWN_AUDIO_CAPACITY 8192u
enum { RC_SID_OWN_6581=0, RC_SID_OWN_8580=1 };
enum { RC_SID_OWN_ATTACK=0, RC_SID_OWN_DECAY=1, RC_SID_OWN_RELEASE=2 };
typedef struct rc_sid_own_voice {
    uint32_t phase, lfsr, test_cycles, floating_cycles;
    uint16_t frequency, pulse_width, waveform, pulse_latch, tri_saw_latch;
    uint16_t rate_counter, rate_period;
    uint8_t control, attack_decay, sustain_release, envelope, envelope_read;
    uint8_t envelope_state, exponential_counter, exponential_period, hold_zero;
    uint8_t noise_pipeline, envelope_pipeline, exponential_pipeline, state_pipeline;
    uint8_t pending_state, rate_reset, msb_rising, previous_state;
    double bitline[12]; /* waveform-selector charge, normalized to its 5 V rail */
} rc_sid_own_voice;
typedef struct rc_sid_own_probe {
    uint32_t phase, lfsr;
    uint16_t waveform;
    uint8_t envelope;
} rc_sid_own_probe;
/* Plain state: no pointers, operating system handles or other chip references. */
typedef struct rc_sid_own {
    rc_sid_own_voice voice[3];
    uint32_t clock_hz, sample_rate, bus_cycles, sample_phase;
    uint64_t cycles;
    uint8_t model, registers[32], bus_value, pot[2];
    uint8_t write_pending, write_reg, write_value, failed, external_filter;
    double filter_low, filter_band, filter_high, external_low, external_high;
    double filter_coefficient, filter_damping, sample_sum, cycle_output;
    double waveform_weight[12], envelope_weight[8], cutoff_weight[12];
    double waveform_bias, cutoff_voltage;
    uint32_t sample_cycles, audio_head, audio_count;
    int16_t audio[RC_SID_OWN_AUDIO_CAPACITY];
} rc_sid_own;
int rc_sid_own_init(rc_sid_own *s,unsigned model,uint32_t clock_hz,uint32_t sample_rate);
void rc_sid_own_reset(rc_sid_own *s);
void rc_sid_own_write(rc_sid_own *s,unsigned reg,uint8_t value);
uint8_t rc_sid_own_read(rc_sid_own *s,unsigned reg);
uint8_t rc_sid_own_peek(const rc_sid_own *s,unsigned reg);
uint8_t rc_sid_own_pot_read(rc_sid_own *s,unsigned reg,uint8_t value);
void rc_sid_own_tick(rc_sid_own *s);
int rc_sid_own_clock(rc_sid_own *s,uint32_t cycles);
size_t rc_sid_own_audio_pull(rc_sid_own *s,int16_t *mono,size_t frames);
void rc_sid_own_get_probe(const rc_sid_own *s,unsigned voice,rc_sid_own_probe *out);
void rc_sid_own_set_external_filter(rc_sid_own *s,int enabled);
int32_t rc_sid_own_output_cycle(const rc_sid_own *s);
/* Writes payload to an already open chunk. Load commits atomically. */
void rc_sid_own_save(const rc_sid_own *s,rc_state_writer *w);
int rc_sid_own_load(rc_sid_own *s,rc_state_reader *r);
#ifdef __cplusplus
}
#endif
#endif

