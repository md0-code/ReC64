#include "drivesnd.h"
#include "drivesnd_data.h"
#include <math.h>
#include <string.h>

/* Every sound is noise through the resonances measured for it, split into
 * eight bands whose levels follow the measured course of the recording. The
 * motor uses one set of resonances for spin-up, running and coast-down, so
 * its states pass into each other without a seam. Synthesis runs at 24 kHz,
 * the bandwidth of the recordings, and is interpolated to the output rate. */
enum { MOTOR_OFF,MOTOR_UP_MODE,MOTOR_RUN_MODE,MOTOR_DOWN_MODE };
enum { VOICE_IDLE,VOICE_STEP_A,VOICE_STEP_B,VOICE_BUMP };
#define ROWS(table) ((unsigned)(sizeof(table)/sizeof(table)[0]))
typedef struct sound_shape {
    const double (*poles)[2];
    const double (*level)[DRIVESND_BANDS];
    unsigned sections, rows, frame;
} sound_shape;
static const sound_shape UP={MOTOR_POLES,MOTOR_UP,ROWS(MOTOR_POLES),ROWS(MOTOR_UP),384};
static const sound_shape DOWN={MOTOR_POLES,MOTOR_DOWN,ROWS(MOTOR_POLES),ROWS(MOTOR_DOWN),384};
static const sound_shape SHAPE[4]={
    {0},
    {STEP_A_POLES,STEP_A,ROWS(STEP_A_POLES),ROWS(STEP_A),36},
    {STEP_B_POLES,STEP_B,ROWS(STEP_B_POLES),ROWS(STEP_B),36},
    {BUMP_POLES,BUMP,ROWS(BUMP_POLES),ROWS(BUMP),72}
};

static double noise(rc_drivesnd *s){
    uint32_t x=s->random;x^=x<<13;x^=x>>17;x^=x<<5;s->random=x;
    /* Uniform with unit variance. */
    return ((double)x/2147483648.0-1.0)*1.7320508075688772;
}
/* Noise through the sound's resonances, then the level of each band. */
static double voice_sample(rc_drivesnd *s,rc_drivesnd_voice *v,const double (*poles)[2],unsigned sections,const double *gain){
    double x=noise(s),sum=0;
    for(unsigned i=0;i<sections;++i){
        const double y=x-poles[i][0]*v->color[i][0]-poles[i][1]*v->color[i][1];
        v->color[i][1]=v->color[i][0];v->color[i][0]=y;x=y;
    }
    unsigned section=0;
    for(unsigned b=0;b<DRIVESND_BANDS;++b){
        double y=x;
        for(unsigned k=b?2u:3u;k;--k,++section){
            const double *c=BANK[section];double *z=v->band[section];
            const double out=c[0]*y+z[0];
            z[0]=c[1]*y-c[3]*out+z[1];z[1]=c[2]*y-c[4]*out;y=out;
        }
        sum+=gain[b]*y;
    }
    return sum;
}
/* The measured rows are level at the centre of their frame. Before the first
 * and after the last the level joins the given neighbouring state. */
static void levels(const sound_shape *p,uint32_t age,const double *before,const double *after,double scale,double *gain){
    const double position=(double)age/p->frame-0.5;
    const double *a,*b;double f;
    if(position<0){a=before;b=p->level[0];f=position*2.0+1.0;}
    else if(position>=p->rows-1u){a=p->level[p->rows-1u];b=after;f=(position-(p->rows-1u))*2.0;}
    else {const unsigned i=(unsigned)position;a=p->level[i];b=p->level[i+1u];f=position-i;}
    for(unsigned k=0;k<DRIVESND_BANDS;++k)gain[k]=scale*((a?a[k]:0.0)+f*((b?b[k]:0.0)-(a?a[k]:0.0)));
}
static void trigger(rc_drivesnd *s,unsigned kind){
    rc_drivesnd_voice *v=&s->voice[s->next_voice];
    memset(v,0,sizeof *v);v->kind=(uint8_t)kind;
    s->next_voice=(uint8_t)((s->next_voice+1u)%RC_DRIVESND_VOICES);
}
static void reset_dev(void *self){rc_drivesnd_reset(self);}
static void save_dev(const void *self,rc_state_writer *w){rc_drivesnd_save(self,w);}
static int load_dev(void *self,rc_state_reader *r){return rc_drivesnd_load(self,r);}
static const rc_dev_ops OPS={.reset=reset_dev,.save=save_dev,.load=load_dev};
void rc_drivesnd_reset(rc_drivesnd *s){
    memset(s,0,sizeof *s);s->dev=(rc_dev){&OPS,NULL,"1541 sound"};s->random=0x1541c64u;s->coast=1.0;
}
void rc_drivesnd_init(rc_drivesnd *s){rc_drivesnd_reset(s);}
void rc_drivesnd_status(rc_drivesnd *s,int motor,uint64_t steps,uint64_t bumps){
    motor=motor!=0;
    if(motor!=s->motor){
        s->motor=(uint8_t)motor;
        if(motor){
            /* A motor still coasting picks up speed again without the start-up. */
            if(s->mode==MOTOR_OFF){s->mode=MOTOR_UP_MODE;s->age=0;}
            else if(s->mode==MOTOR_DOWN_MODE){s->mode=MOTOR_RUN_MODE;s->age=0;}
        }else if(s->mode==MOTOR_UP_MODE||s->mode==MOTOR_RUN_MODE){
            /* Stopped while starting: coast down from the speed reached. */
            s->coast=s->mode==MOTOR_UP_MODE?(double)s->age/(UP.rows*UP.frame):1.0;
            s->mode=MOTOR_DOWN_MODE;s->age=0;
        }
    }
    uint64_t n=steps>=s->steps?steps-s->steps:0;
    if(n>RC_DRIVESND_VOICES)n=RC_DRIVESND_VOICES;
    /* The two measured head-step timbres alternate with physical steps. */
    for(uint64_t i=n;i>0;--i)trigger(s,VOICE_STEP_A+(unsigned)((steps-i)&1u));
    s->steps=steps;
    n=bumps>=s->bumps?bumps-s->bumps:0;
    if(n>RC_DRIVESND_VOICES)n=RC_DRIVESND_VOICES;
    for(uint64_t i=0;i<n;++i)trigger(s,VOICE_BUMP);
    s->bumps=bumps;
}
static double motor_sample(rc_drivesnd *s){
    double target[DRIVESND_BANDS];
    if(s->mode==MOTOR_OFF)return 0;
    if(s->mode==MOTOR_UP_MODE){
        levels(&UP,s->age,NULL,MOTOR_RUN[0],1.0,target);
        if(++s->age>=UP.rows*UP.frame){s->mode=MOTOR_RUN_MODE;s->age=0;}
    }else if(s->mode==MOTOR_RUN_MODE){
        for(unsigned b=0;b<DRIVESND_BANDS;++b)target[b]=MOTOR_RUN[0][b];
    }else {
        levels(&DOWN,s->age,MOTOR_DOWN[0],NULL,s->coast,target);
        if(++s->age>=DOWN.rows*DOWN.frame){
            s->mode=MOTOR_OFF;s->age=0;s->coast=1.0;
            memset(&s->motor_voice,0,sizeof s->motor_voice);memset(s->gain,0,sizeof s->gain);
            return 0;
        }
    }
    /* A few milliseconds of slew keep a change of state free of clicks. */
    for(unsigned b=0;b<DRIVESND_BANDS;++b)s->gain[b]+=(target[b]-s->gain[b])/64.0;
    return voice_sample(s,&s->motor_voice,MOTOR_POLES,ROWS(MOTOR_POLES),s->gain);
}
int16_t rc_drivesnd_sample(rc_drivesnd *s){
    double sample;
    if(!s->half){
        double mix=motor_sample(s);
        for(unsigned j=0;j<RC_DRIVESND_VOICES;++j){
            rc_drivesnd_voice *v=&s->voice[j];if(!v->kind)continue;
            const sound_shape *p=&SHAPE[v->kind];double gain[DRIVESND_BANDS];
            levels(p,v->age,NULL,NULL,1.0,gain);
            mix+=voice_sample(s,v,p->poles,p->sections,gain);
            if(++v->age>=p->rows*p->frame)memset(v,0,sizeof *v);
        }
        s->previous=s->current;s->current=mix;
        sample=0.5*(s->previous+s->current);
    }else sample=s->current;
    s->half^=1u;
    sample=floor(sample+0.5);
    if(sample>32767)sample=32767;else if(sample<-32768)sample=-32768;
    return (int16_t)sample;
}
static void write_double(rc_state_writer *w,double value){uint64_t bits;memcpy(&bits,&value,sizeof bits);rc_sw_u64(w,bits);}
static double read_double(rc_state_reader *r,int *ok){
    uint64_t bits=rc_sr_u64(r);double value;memcpy(&value,&bits,sizeof value);if(!isfinite(value))*ok=0;return value;
}
static void save_voice(const rc_drivesnd_voice *v,rc_state_writer *w){
    rc_sw_u8(w,v->kind);rc_sw_u32(w,v->age);
    for(unsigned i=0;i<RC_DRIVESND_SECTIONS;++i){write_double(w,v->color[i][0]);write_double(w,v->color[i][1]);}
    for(unsigned i=0;i<RC_DRIVESND_BANK;++i){write_double(w,v->band[i][0]);write_double(w,v->band[i][1]);}
}
static void load_voice(rc_drivesnd_voice *v,rc_state_reader *r,int *ok){
    v->kind=rc_sr_u8(r);v->age=rc_sr_u32(r);
    for(unsigned i=0;i<RC_DRIVESND_SECTIONS;++i){v->color[i][0]=read_double(r,ok);v->color[i][1]=read_double(r,ok);}
    for(unsigned i=0;i<RC_DRIVESND_BANK;++i){v->band[i][0]=read_double(r,ok);v->band[i][1]=read_double(r,ok);}
}
void rc_drivesnd_save(const rc_drivesnd *s,rc_state_writer *w){
    rc_sw_u32(w,3);rc_sw_u64(w,s->steps);rc_sw_u64(w,s->bumps);
    rc_sw_u32(w,s->random);rc_sw_u32(w,s->age);
    rc_sw_u8(w,s->motor);rc_sw_u8(w,s->mode);rc_sw_u8(w,s->next_voice);rc_sw_u8(w,s->half);
    for(unsigned b=0;b<DRIVESND_BANDS;++b)write_double(w,s->gain[b]);
    write_double(w,s->coast);write_double(w,s->previous);write_double(w,s->current);
    save_voice(&s->motor_voice,w);
    for(unsigned j=0;j<RC_DRIVESND_VOICES;++j)save_voice(&s->voice[j],w);
}
int rc_drivesnd_load(rc_drivesnd *s,rc_state_reader *r){
    rc_drivesnd t;rc_drivesnd_init(&t);int ok=1;
    const uint32_t payload=rc_sr_u32(r);
    t.steps=rc_sr_u64(r);t.bumps=rc_sr_u64(r);
    if(payload==1){
        /* The earlier oscillator model: keep the mechanism counters and the
         * motor, and let the sound start afresh. */
        for(unsigned i=0;i<8;++i)(void)rc_sr_u32(r);
        t.motor=rc_sr_u8(r);(void)rc_sr_u8(r);(void)rc_sr_u8(r);
        for(unsigned j=0;j<8;++j){for(unsigned i=0;i<5;++i)(void)rc_sr_u32(r);(void)rc_sr_u8(r);}
        if(!rc_sr_ok(r)||t.motor>1)return 0;
        if(t.motor){t.mode=MOTOR_RUN_MODE;for(unsigned b=0;b<DRIVESND_BANDS;++b)t.gain[b]=MOTOR_RUN[0][b];}
        *s=t;return 1;
    }
    if(payload!=3)return 0;
    t.random=rc_sr_u32(r);t.age=rc_sr_u32(r);
    t.motor=rc_sr_u8(r);t.mode=rc_sr_u8(r);t.next_voice=rc_sr_u8(r);t.half=rc_sr_u8(r);
    for(unsigned b=0;b<DRIVESND_BANDS;++b)t.gain[b]=read_double(r,&ok);
    t.coast=read_double(r,&ok);t.previous=read_double(r,&ok);t.current=read_double(r,&ok);
    load_voice(&t.motor_voice,r,&ok);
    for(unsigned j=0;j<RC_DRIVESND_VOICES;++j){
        rc_drivesnd_voice *v=&t.voice[j];load_voice(v,r,&ok);
        if(v->kind>VOICE_BUMP||(v->kind?v->age>=SHAPE[v->kind].rows*SHAPE[v->kind].frame:v->age!=0))return 0;
    }
    if(!ok||!rc_sr_ok(r)||!t.random||t.motor>1||t.mode>MOTOR_DOWN_MODE||t.next_voice>=RC_DRIVESND_VOICES||t.half>1||
       t.motor_voice.kind||t.motor_voice.age||t.coast<0||t.coast>1)return 0;
    if((t.mode==MOTOR_UP_MODE&&t.age>=UP.rows*UP.frame)||(t.mode==MOTOR_DOWN_MODE&&t.age>=DOWN.rows*DOWN.frame)||
       ((t.mode==MOTOR_OFF||t.mode==MOTOR_RUN_MODE)&&t.age))return 0;
    if((t.mode==MOTOR_UP_MODE||t.mode==MOTOR_RUN_MODE)!=!!t.motor)return 0;
    *s=t;return 1;
}
