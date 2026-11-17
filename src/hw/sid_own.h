/* Original SID engine; hardware sources are pinned in docs/sources.lock.json. */
#ifndef RC_SID_OWN_H
#define RC_SID_OWN_H
#include <stddef.h>
#include <stdint.h>
#include "../core/state.h"
#ifdef __cplusplus
extern "C" {
#endif
#define RC_SID_OWN_AUDIO_CAPACITY 8192u
#define RC_SID_OWN_HISTORY 1024u      /* decimated samples kept for the output kernel */
#define RC_SID_OWN_KERNEL 4096u       /* kernel table points over its half width */
#define RC_SID_OWN_CURVE 1024u        /* cutoff curve points over the gate drive range */
enum { RC_SID_OWN_6581=0, RC_SID_OWN_8580=1 };
enum { RC_SID_OWN_ATTACK=0, RC_SID_OWN_DECAY=1, RC_SID_OWN_RELEASE=2 };
typedef struct rc_sid_own_voice {
    uint32_t phase, lfsr, test_cycles, floating_cycles;
    uint16_t frequency, pulse_width, waveform, pulse_latch, tri_saw_latch, osc3;
    uint16_t rate_counter, rate_period;
    uint8_t control, attack_decay, sustain_release, envelope, envelope_read;
    uint8_t envelope_state, exponential_counter, exponential_period, hold_zero;
    uint8_t noise_pipeline, envelope_pipeline, exponential_pipeline, state_pipeline;
    uint8_t pending_state, rate_reset, msb_rising;
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
    double cycle_output;
    /* Output resampling: a short triangular pre-filter to a quarter of the
     * chip clock, then a windowed-sinc kernel at the host rate. */
    double recent[8];
    float history[RC_SID_OWN_HISTORY];
    uint32_t recent_head, history_head;
    uint32_t audio_head, audio_count;
    int16_t audio[RC_SID_OWN_AUDIO_CAPACITY];
    /* Derived from the model, the rates and the registers; never saved. */
    double filter_coefficient, filter_damping, volume_gain, cutoff_gate, cutoff_scale;
    double cutoff_weight[11];
    double external_lp, external_hp, mixer_scale[8];
    float waveform_level[4096], envelope_level[256];
    float kernel[RC_SID_OWN_KERNEL+2];
    float cutoff_curve[RC_SID_OWN_CURVE+2];
    uint16_t combined[6][4096];   /* triangle+saw, pulse+triangle, pulse+saw and all three */
    uint32_t kernel_span;
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
