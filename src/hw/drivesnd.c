#include "drivesnd.h"
#include <string.h>

/* Scalar measurements, not PCM: docs/sources.lock.json, drive_recordings.
 * Frequencies rounded to Hz; original event durations rounded to 48 kHz.
 * Recorded 8-bit levels are scaled by 128 into the 16-bit mixer. */
enum { SPINUP=17635, SPINDOWN=16285, GAIN=65536 };
typedef struct sound_shape {
    uint16_t hz[3], weight[3], level, decay;
    uint32_t samples;
} sound_shape;
static const sound_shape SHAPE[3]={
    {{2165,622,2000},{1000,851,821},1984,137,1504},
    {{935,442,534},{1000,834,783},715,55,1482},
    {{203,267,2073},{1000,463,252},8192,46,5695}
};

/* An algebraic sine approximation in Q15; unsigned phase wraps naturally.
 * Explicit division keeps negative rounding identical on every C11 host. */
static int32_t wave(uint32_t phase){
    int32_t x=(int32_t)(phase>>16);if(x>=32768)x-=65536;
    const int32_t ax=x<0?-x:x;
    int32_t y=(int32_t)((int64_t)x*(32768-ax)*4/32768);
    const int32_t ay=y<0?-y:y;
    y+=(int32_t)(225*((int64_t)y*ay/32768-y)/1000);
    return y;
}
static uint32_t increment(uint32_t hz){return (uint32_t)(((uint64_t)hz<<32)/RC_DRIVESND_RATE);}
static uint32_t random_next(rc_drivesnd *s){
    uint32_t x=s->random;x^=x<<13;x^=x>>17;x^=x<<5;return s->random=x;
}
static void trigger(rc_drivesnd *s,unsigned kind){
    rc_drivesnd_voice *v=&s->voice[s->next_voice];
    memset(v,0,sizeof *v);v->kind=(uint8_t)kind;
    v->envelope=1u<<24;v->remaining=SHAPE[kind].samples;
    s->next_voice=(uint8_t)((s->next_voice+1u)%RC_DRIVESND_VOICES);
}
static void reset_dev(void *self){rc_drivesnd_reset(self);}
static void save_dev(const void *self,rc_state_writer *w){rc_drivesnd_save(self,w);}
static int load_dev(void *self,rc_state_reader *r){return rc_drivesnd_load(self,r);}
static const rc_dev_ops OPS={.reset=reset_dev,.save=save_dev,.load=load_dev};
void rc_drivesnd_reset(rc_drivesnd *s){
    memset(s,0,sizeof *s);s->dev=(rc_dev){&OPS,NULL,"1541 sound"};s->random=0x1541c64u;
}
void rc_drivesnd_init(rc_drivesnd *s){rc_drivesnd_reset(s);}
void rc_drivesnd_status(rc_drivesnd *s,int motor,uint64_t steps,uint64_t bumps){
    motor=motor!=0;
    if(motor!=s->motor){s->motor=(uint8_t)motor;s->mode=(uint8_t)(motor?1:3);s->age=0;s->start_gain=s->gain;}
    uint64_t n=steps>=s->steps?steps-s->steps:0;
    if(n>RC_DRIVESND_VOICES)n=RC_DRIVESND_VOICES;
    /* The two measured head-step timbres alternate with physical steps. */
    for(uint64_t i=n;i>0;--i)trigger(s,(unsigned)((steps-i)&1u));
    s->steps=steps;
    n=bumps>=s->bumps?bumps-s->bumps:0;
    if(n>RC_DRIVESND_VOICES)n=RC_DRIVESND_VOICES;
    for(uint64_t i=0;i<n;++i)trigger(s,2);
    s->bumps=bumps;
}

/* Coarse RMS envelope measurements at the centres of 1102-sample windows.
 * Values are gain relative to the measured steady hum RMS of 5.7414 PCM.
 * The last point joins the steady motor at the recorded spin-up duration. */
static uint32_t startup_gain(uint32_t age){
    static const uint16_t at[]={0,1199,3598,5997,8396,10795,13194,15593,SPINUP};
    static const uint32_t value[]={0,6061,8264,83932,51093,86672,59664,53799,GAIN};
    unsigned i=1;while(i<8&&age>at[i])++i;
    return (uint32_t)((int64_t)value[i-1]+((int64_t)value[i]-value[i-1])*(age-at[i-1])/(at[i]-at[i-1]));
}
int16_t rc_drivesnd_sample(rc_drivesnd *s){
    uint32_t speed=GAIN;
    if(s->mode==1){
        speed=(uint32_t)((uint64_t)s->age*GAIN/SPINUP);
        const uint32_t measured=startup_gain(s->age);
        /* Preserve continuity when a stopped/coasting motor is restarted. */
        s->gain=measured+(uint32_t)((uint64_t)s->start_gain*(GAIN-speed)/GAIN);
        if(s->gain>2u*GAIN)s->gain=2u*GAIN;
        if(++s->age>=SPINUP){s->mode=2;s->age=0;s->gain=GAIN;}
    }else if(s->mode==2)s->gain=GAIN;
    else if(s->mode==3){
        /* Measured first e-fold: 89.796 ms = 4310 output samples. */
        s->gain=(uint32_t)((uint64_t)s->gain*65521u/65536u);
        speed=GAIN-(uint32_t)((uint64_t)s->age*(GAIN/2)/SPINDOWN);
        if(++s->age>=SPINDOWN){s->mode=0;s->age=0;s->gain=0;}
    }else s->gain=0;
    int64_t sample=0;
    if(s->gain){
        const uint32_t hz[3]={1929u+141u*speed/GAIN,205u,2119u+45u*speed/GAIN};
        const uint16_t weight[3]={1000,948,802};
        int64_t tones=0;
        for(unsigned i=0;i<3;++i){s->phase[i]+=increment(hz[i]);tones+=(int64_t)wave(s->phase[i])*weight[i];}
        const int32_t white=(int32_t)(random_next(s)>>16)-32768;
        s->noise+=(white-s->noise)/4;
        sample=((tones*600/(1000*32768)+(int64_t)s->noise*1300/32768)*s->gain)/GAIN;
    }
    for(unsigned j=0;j<RC_DRIVESND_VOICES;++j){
        rc_drivesnd_voice *v=&s->voice[j];if(!v->remaining)continue;
        const sound_shape *p=&SHAPE[v->kind];int64_t tones=0;
        for(unsigned i=0;i<3;++i){v->phase[i]+=increment(p->hz[i]);tones+=(int64_t)wave(v->phase[i])*p->weight[i];}
        sample+=((tones*p->level/(1000*32768))*v->envelope)/(1u<<24);
        v->envelope=(uint32_t)((uint64_t)v->envelope*(65536u-p->decay)/65536u);
        --v->remaining;
    }
    if(sample>32767)sample=32767;else if(sample<-32768)sample=-32768;
    return (int16_t)sample;
}
void rc_drivesnd_save(const rc_drivesnd *s,rc_state_writer *w){
    rc_sw_u32(w,1);rc_sw_u64(w,s->steps);rc_sw_u64(w,s->bumps);
    for(unsigned i=0;i<3;++i)rc_sw_u32(w,s->phase[i]);
    rc_sw_u32(w,s->random);rc_sw_u32(w,s->age);rc_sw_u32(w,s->gain);rc_sw_u32(w,s->start_gain);
    rc_sw_u32(w,(uint32_t)s->noise);rc_sw_u8(w,s->motor);rc_sw_u8(w,s->mode);rc_sw_u8(w,s->next_voice);
    for(unsigned j=0;j<RC_DRIVESND_VOICES;++j){
        const rc_drivesnd_voice *v=&s->voice[j];
        for(unsigned i=0;i<3;++i)rc_sw_u32(w,v->phase[i]);
        rc_sw_u32(w,v->envelope);rc_sw_u32(w,v->remaining);rc_sw_u8(w,v->kind);
    }
}
int rc_drivesnd_load(rc_drivesnd *s,rc_state_reader *r){
    rc_drivesnd t;rc_drivesnd_init(&t);if(rc_sr_u32(r)!=1)return 0;
    t.steps=rc_sr_u64(r);t.bumps=rc_sr_u64(r);
    for(unsigned i=0;i<3;++i)t.phase[i]=rc_sr_u32(r);
    t.random=rc_sr_u32(r);t.age=rc_sr_u32(r);t.gain=rc_sr_u32(r);t.start_gain=rc_sr_u32(r);
    const uint32_t noise=rc_sr_u32(r);
    /* Reconstruct signed noise without an out-of-range unsigned cast. */
    if(noise<=32767)t.noise=(int32_t)noise;
    else if(noise>=UINT32_MAX-32767)t.noise=-(int32_t)(UINT32_MAX-noise)-1;
    else return 0;
    t.motor=rc_sr_u8(r);t.mode=rc_sr_u8(r);t.next_voice=rc_sr_u8(r);
    if(!t.random||t.noise<-32768||t.noise>32767||t.motor>1||t.mode>3||t.next_voice>=RC_DRIVESND_VOICES||t.gain>3u*GAIN||t.start_gain>3u*GAIN)return 0;
    if((t.mode==1&&t.age>=SPINUP)||(t.mode==3&&t.age>=SPINDOWN)||((t.mode==0||t.mode==2)&&t.age))return 0;
    if((t.mode==1||t.mode==2)!=!!t.motor)return 0;
    for(unsigned j=0;j<RC_DRIVESND_VOICES;++j){
        rc_drivesnd_voice *v=&t.voice[j];
        for(unsigned i=0;i<3;++i)v->phase[i]=rc_sr_u32(r);
        v->envelope=rc_sr_u32(r);v->remaining=rc_sr_u32(r);v->kind=rc_sr_u8(r);
        if(v->kind>2||v->remaining>SHAPE[v->kind].samples||v->envelope>(1u<<24))return 0;
    }
    if(!rc_sr_ok(r))return 0;*s=t;return 1;
}
