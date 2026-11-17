/* C interface to selectable SID engines. No backend types escape here. */
#ifndef RC_SID_H
#define RC_SID_H
#include "hw.h"
#include "../core/state.h"
#ifdef __cplusplus
extern "C" {
#endif
enum {RC_SID_6581,RC_SID_8580};
enum {RC_SID_RESID,RC_SID_BUILTIN};
typedef struct rc_sid {rc_dev dev;void *engine;unsigned backend;} rc_sid;
int rc_sid_init(rc_sid *s,const rc_hw_host *host,unsigned model,uint32_t clock_hz,uint32_t sample_rate);
int rc_sid_init_backend(rc_sid *s,const rc_hw_host *host,unsigned model,uint32_t clock_hz,uint32_t sample_rate,unsigned backend);
void rc_sid_power(rc_sid *s);
void rc_sid_release(rc_sid *s);
void rc_sid_reset(rc_sid *s);
uint8_t rc_sid_read(rc_sid *s,unsigned reg);
/* External ADC result, with the same data-bus retention as a normal read. */
uint8_t rc_sid_pot_read(rc_sid *s,unsigned reg,uint8_t value);
uint8_t rc_sid_peek(rc_sid *s,unsigned reg);
void rc_sid_write(rc_sid *s,unsigned reg,uint8_t value);
int rc_sid_clock(rc_sid *s,uint32_t cycles);
size_t rc_sid_audio_pull(rc_sid *s,int16_t *mono,size_t frames);
int rc_sid_ok(const rc_sid *s);
void rc_sid_save(const rc_sid *s,rc_state_writer *w);
int rc_sid_load(rc_sid *s,rc_state_reader *r);
#ifdef __cplusplus
}
#endif
#endif
