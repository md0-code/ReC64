/* Chip interface of the SID: allocation, device operations and the snapshot
 * payload around the engine of sid_own.c. */
#include "sid.h"
#include "sid_own.h"
#include <stdlib.h>

static uint8_t read_dev(void *p,unsigned space,uint32_t off){(void)space;return rc_sid_read(p,off);}
static void write_dev(void *p,unsigned space,uint32_t off,uint8_t v){(void)space;rc_sid_write(p,off,v);}
static void reset_dev(void *p){rc_sid_reset(p);}
static void save_dev(const void *p,rc_state_writer *w){rc_sid_save(p,w);}
static int load_dev(void *p,rc_state_reader *r){return rc_sid_load(p,r);}
static const rc_dev_ops OPS={.read=read_dev,.write=write_dev,.reset=reset_dev,.save=save_dev,.load=load_dev};
int rc_sid_init(rc_sid *s,const rc_hw_host *host,unsigned model,uint32_t hz,uint32_t rate){
    s->dev=(rc_dev){&OPS,host,"SID"};s->engine=NULL;
    if(model>RC_SID_8580)return 0;
    rc_sid_own *e=calloc(1,sizeof *e);
    if(!e||!rc_sid_own_init(e,model,hz,rate)){free(e);return 0;}
    s->engine=e;return 1;
}
void rc_sid_release(rc_sid *s){free(s->engine);s->engine=NULL;}
void rc_sid_reset(rc_sid *s){
    rc_sid_own *e=s->engine;if(!e)return;
    rc_sid_own_reset(e);e->audio_head=e->audio_count=0;e->failed=0;
}
void rc_sid_power(rc_sid *s){
    rc_sid_own *e=s->engine;if(!e)return;
    const unsigned model=e->model;const uint32_t hz=e->clock_hz,rate=e->sample_rate;
    rc_sid_own_init(e,model,hz,rate);
}
uint8_t rc_sid_read(rc_sid *s,unsigned reg){return s->engine?rc_sid_own_read(s->engine,reg):255;}
uint8_t rc_sid_pot_read(rc_sid *s,unsigned reg,uint8_t value){return s->engine?rc_sid_own_pot_read(s->engine,reg,value):255;}
uint8_t rc_sid_peek(rc_sid *s,unsigned reg){return s->engine?rc_sid_own_peek(s->engine,reg):255;}
void rc_sid_write(rc_sid *s,unsigned reg,uint8_t v){if(s->engine)rc_sid_own_write(s->engine,reg,v);}
int rc_sid_clock(rc_sid *s,uint32_t cycles){return s->engine&&rc_sid_own_clock(s->engine,cycles);}
size_t rc_sid_audio_pull(rc_sid *s,int16_t *mono,size_t frames){return s->engine?rc_sid_own_audio_pull(s->engine,mono,frames):0;}
int rc_sid_ok(const rc_sid *s){const rc_sid_own *e=s->engine;return e&&!e->failed;}
void rc_sid_save(const rc_sid *s,rc_state_writer *w){
    const rc_sid_own *e=s->engine;if(!e||e->failed){w->failed=1;return;}
    rc_sw_u32(w,2);rc_sw_u32(w,e->model);rc_sw_u32(w,e->clock_hz);rc_sw_u32(w,e->sample_rate);rc_sid_own_save(e,w);
}
/* Payload 1 is a snapshot of the releases that ran the reSID library: a
 * journal of bus accesses and clock runs. Its register writes are replayed
 * without their timing, so a restored program keeps its volume, filter and
 * voice settings and its notes start again. */
static int load_journal(rc_sid_own *e,rc_state_reader *r){
    const uint32_t count=rc_sr_u32(r);if(count>(uint32_t)(r->end-r->at)/7u)return 0;
    const unsigned model=e->model;const uint32_t hz=e->clock_hz,rate=e->sample_rate;
    rc_sid_own_init(e,model,hz,rate);
    for(uint32_t i=0;i<count;++i){
        const uint8_t kind=rc_sr_u8(r),reg=rc_sr_u8(r),value=rc_sr_u8(r);(void)rc_sr_u32(r);
        if(kind<1||kind>3||reg>31)return 0;
        if(kind==2)rc_sid_own_write(e,reg,value);
    }
    rc_sid_own_tick(e);e->audio_head=e->audio_count=0;
    const uint32_t samples=rc_sr_u32(r);if(samples>(uint32_t)(r->end-r->at)/2u)return 0;
    for(uint32_t i=0;i<samples;++i)(void)rc_sr_u16(r);
    if(r->end-r->at>=5){(void)rc_sr_u8(r);(void)rc_sr_u32(r);}
    return rc_sr_ok(r);
}
int rc_sid_load(rc_sid *s,rc_state_reader *r){
    rc_sid_own *e=s->engine;if(!e||r->end-r->at<16)return 0;
    const uint32_t payload=rc_sr_u32(r),model=rc_sr_u32(r),hz=rc_sr_u32(r),rate=rc_sr_u32(r);
    if(model!=e->model||hz!=e->clock_hz||rate!=e->sample_rate)return 0;
    return payload==2?rc_sid_own_load(e,r):payload==1&&load_journal(e,r);
}
