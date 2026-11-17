#include "drivesnd.h"
#include "drivesnd_data.h"
#include <math.h>
#include <string.h>

/* A head step or a hit on the head stop is a set of measured resonances
 * struck once: each turns and decays by its own factor every sample. The
 * running motor is one measured loop, kept as its strongest lines, plus
 * noise through the spectrum of what the lines leave. Spin-up and
 * coast-down read the same loop at the motor's speed and follow the
 * measured level, so the states pass into each other without a seam. */
enum { MOTOR_OFF,MOTOR_UP_MODE,MOTOR_RUN_MODE,MOTOR_DOWN_MODE };
enum { VOICE_IDLE,VOICE_STEP_A,VOICE_STEP_B,VOICE_BUMP };
#define ROWS(table) ((unsigned)(sizeof(table)/sizeof(table)[0]))
#define TAIL 960u       /* the resonances ring out for 20 ms past the measured length */
#define SETTLE 512u     /* the motor's last level fades before it falls silent */
#define LEVEL_SLEW 128.0
#define SPEED_SLEW 256.0
typedef struct strike_shape {
    const double (*mode)[4];
    unsigned modes, length;
} strike_shape;
static const strike_shape SHAPE[4]={
    {0},
    {STEP_A,ROWS(STEP_A),STEP_A_LENGTH},
    {STEP_B,ROWS(STEP_B),STEP_B_LENGTH},
    {BUMP,ROWS(BUMP),BUMP_LENGTH}
};
_Static_assert(ROWS(STEP_A)<=RC_DRIVESND_MODES&&ROWS(STEP_B)<=RC_DRIVESND_MODES&&ROWS(BUMP)<=RC_DRIVESND_MODES,"resonances per sound");
_Static_assert(ROWS(MOTOR_NOISE)==RC_DRIVESND_NOISE,"noise filter order");

/* One loop of the running motor, from its lines; the first sample repeats at the end. */
static double LOOP[DRIVESND_LOOP+1u];
static int loop_ready;
static void build_loop(void){
    if(loop_ready)return;
    for(unsigned i=0;i<ROWS(MOTOR_LINES);++i){
        const double angle=6.283185307179586476925*MOTOR_LINES[i][0]/DRIVESND_LOOP,c=cos(angle),d=sin(angle);
        double x=1,y=0;
        for(unsigned n=0;n<DRIVESND_LOOP;++n){
            LOOP[n]+=MOTOR_LINES[i][1]*x-MOTOR_LINES[i][2]*y;
            const double t=x*c-y*d;y=x*d+y*c;x=t;
        }
    }
    LOOP[DRIVESND_LOOP]=LOOP[0];loop_ready=1;
}
static double noise(rc_drivesnd *s){
    uint32_t x=s->random;x^=x<<13;x^=x>>17;x^=x<<5;s->random=x;
    /* Uniform with unit variance. */
    return ((double)x/2147483648.0-1.0)*1.7320508075688772;
}
static void strike(rc_drivesnd_voice *v,unsigned kind){
    const strike_shape *p=&SHAPE[kind];
    memset(v,0,sizeof *v);v->kind=(uint8_t)kind;
    for(unsigned i=0;i<p->modes;++i){v->mode[i][0]=p->mode[i][2];v->mode[i][1]=p->mode[i][3];}
}
/* One sample of a struck sound; the voice falls idle after its last. */
static double voice_sample(rc_drivesnd_voice *v){
    const strike_shape *p=&SHAPE[v->kind];double sum=0;
    for(unsigned i=0;i<p->modes;++i){
        const double x=v->mode[i][0],y=v->mode[i][1];
        sum+=x;v->mode[i][0]=x*p->mode[i][0]-y*p->mode[i][1];v->mode[i][1]=x*p->mode[i][1]+y*p->mode[i][0];
    }
    if(v->age>=p->length)sum*=1.0-(double)(v->age-p->length)/(TAIL-1u);
    if(++v->age>=p->length+TAIL)memset(v,0,sizeof *v);
    return sum;
}
static void trigger(rc_drivesnd *s,unsigned kind){
    strike(&s->voice[s->next_voice],kind);
    s->next_voice=(uint8_t)((s->next_voice+1u)%RC_DRIVESND_VOICES);
}
/* The measured levels are those at the centres of their frames. */
static double course(const double *level,unsigned rows,unsigned frame,uint32_t age){
    const double position=(double)age/frame-0.5;
    if(position<=0)return level[0];
    if(position>=rows-1u)return level[rows-1u];
    const unsigned i=(unsigned)position;
    return level[i]+(position-i)*(level[i+1u]-level[i]);
}
static void motor_stop(rc_drivesnd *s){
    s->mode=MOTOR_OFF;s->age=0;s->level=s->speed=s->turn=0;s->coast=1.0;memset(s->hiss,0,sizeof s->hiss);
}
static void reset_dev(void *self){rc_drivesnd_reset(self);}
static void save_dev(const void *self,rc_state_writer *w){rc_drivesnd_save(self,w);}
static int load_dev(void *self,rc_state_reader *r){return rc_drivesnd_load(self,r);}
static const rc_dev_ops OPS={.reset=reset_dev,.save=save_dev,.load=load_dev};
void rc_drivesnd_reset(rc_drivesnd *s){
    build_loop();
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
            s->coast=s->speed<1.0?s->speed:1.0;
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
    double level=1.0,speed=1.0;
    if(s->mode==MOTOR_OFF)return 0;
    if(s->mode==MOTOR_UP_MODE){
        level=course(MOTOR_UP,ROWS(MOTOR_UP),MOTOR_UP_FRAME,s->age);
        speed=1.6*s->age/MOTOR_UP_LENGTH;if(speed>1.0)speed=1.0;
        if(++s->age>=MOTOR_UP_LENGTH){s->mode=MOTOR_RUN_MODE;s->age=0;}
    }else if(s->mode==MOTOR_DOWN_MODE){
        if(s->age<MOTOR_DOWN_LENGTH){
            level=s->coast*course(MOTOR_DOWN,ROWS(MOTOR_DOWN),MOTOR_DOWN_FRAME,s->age);
            speed=s->coast*(1.0-0.75*s->age/(MOTOR_DOWN_LENGTH-1u));
        }else {level=0;speed=s->coast*0.25;}
        if(++s->age>=MOTOR_DOWN_LENGTH+SETTLE){motor_stop(s);return 0;}
    }
    /* A few milliseconds of slew keep a change of state free of clicks. */
    s->level+=(level-s->level)/LEVEL_SLEW;s->speed+=(speed-s->speed)/SPEED_SLEW;
    const unsigned at=(unsigned)s->turn;const double f=s->turn-at;
    double sample=LOOP[at]+f*(LOOP[at+1u]-LOOP[at]);
    s->turn+=s->speed;if(s->turn>=DRIVESND_LOOP)s->turn-=DRIVESND_LOOP;
    double hiss=MOTOR_NOISE_GAIN*noise(s);
    for(unsigned i=0;i<RC_DRIVESND_NOISE;++i)hiss+=MOTOR_NOISE[i]*s->hiss[i];
    memmove(s->hiss+1,s->hiss,(RC_DRIVESND_NOISE-1u)*sizeof s->hiss[0]);s->hiss[0]=hiss;
    return s->level*(sample+hiss);
}
int16_t rc_drivesnd_sample(rc_drivesnd *s){
    double sample=motor_sample(s);
    for(unsigned j=0;j<RC_DRIVESND_VOICES;++j)if(s->voice[j].kind)sample+=voice_sample(&s->voice[j]);
    sample=floor(sample+0.5);
    if(sample>32767)sample=32767;else if(sample<-32768)sample=-32768;
    return (int16_t)sample;
}
static void write_double(rc_state_writer *w,double value){uint64_t bits;memcpy(&bits,&value,sizeof bits);rc_sw_u64(w,bits);}
static double read_double(rc_state_reader *r,int *ok){
    uint64_t bits=rc_sr_u64(r);double value;memcpy(&value,&bits,sizeof value);if(!isfinite(value))*ok=0;return value;
}
void rc_drivesnd_save(const rc_drivesnd *s,rc_state_writer *w){
    rc_sw_u32(w,4);rc_sw_u64(w,s->steps);rc_sw_u64(w,s->bumps);
    rc_sw_u32(w,s->random);rc_sw_u32(w,s->age);
    rc_sw_u8(w,s->motor);rc_sw_u8(w,s->mode);rc_sw_u8(w,s->next_voice);
    write_double(w,s->level);write_double(w,s->speed);write_double(w,s->coast);write_double(w,s->turn);
    for(unsigned i=0;i<RC_DRIVESND_NOISE;++i)write_double(w,s->hiss[i]);
    /* A struck sound is its kind and its age: the resonances follow from them. */
    for(unsigned j=0;j<RC_DRIVESND_VOICES;++j){rc_sw_u8(w,s->voice[j].kind);rc_sw_u32(w,s->voice[j].age);}
}
int rc_drivesnd_load(rc_drivesnd *s,rc_state_reader *r){
    rc_drivesnd t;rc_drivesnd_init(&t);int ok=1;
    const uint32_t payload=rc_sr_u32(r);
    t.steps=rc_sr_u64(r);t.bumps=rc_sr_u64(r);
    if(payload==1||payload==3){
        /* The earlier models: keep the mechanism counters and the motor, and
         * let the sound start afresh. */
        if(payload==1){
            for(unsigned i=0;i<8;++i)(void)rc_sr_u32(r);
            t.motor=rc_sr_u8(r);(void)rc_sr_u8(r);(void)rc_sr_u8(r);
            for(unsigned j=0;j<8;++j){for(unsigned i=0;i<5;++i)(void)rc_sr_u32(r);(void)rc_sr_u8(r);}
        }else {
            (void)rc_sr_u32(r);(void)rc_sr_u32(r);
            t.motor=rc_sr_u8(r);(void)rc_sr_u8(r);(void)rc_sr_u8(r);(void)rc_sr_u8(r);
            for(unsigned i=0;i<11;++i)(void)rc_sr_u64(r);
            for(unsigned j=0;j<9;++j){(void)rc_sr_u8(r);(void)rc_sr_u32(r);for(unsigned i=0;i<66;++i)(void)rc_sr_u64(r);}
        }
        if(!rc_sr_ok(r)||t.motor>1)return 0;
        if(t.motor){t.mode=MOTOR_RUN_MODE;t.level=t.speed=1.0;}
        *s=t;return 1;
    }
    if(payload!=4)return 0;
    t.random=rc_sr_u32(r);t.age=rc_sr_u32(r);
    t.motor=rc_sr_u8(r);t.mode=rc_sr_u8(r);t.next_voice=rc_sr_u8(r);
    t.level=read_double(r,&ok);t.speed=read_double(r,&ok);t.coast=read_double(r,&ok);t.turn=read_double(r,&ok);
    for(unsigned i=0;i<RC_DRIVESND_NOISE;++i)t.hiss[i]=read_double(r,&ok);
    for(unsigned j=0;j<RC_DRIVESND_VOICES;++j){
        const unsigned kind=rc_sr_u8(r);const uint32_t age=rc_sr_u32(r);
        if(!rc_sr_ok(r)||kind>VOICE_BUMP||(kind?age>=SHAPE[kind].length+TAIL:age!=0))return 0;
        if(!kind)continue;
        /* The same steps as the run that was saved give the same resonances, bit for bit. */
        strike(&t.voice[j],kind);for(uint32_t i=0;i<age;++i)(void)voice_sample(&t.voice[j]);
    }
    if(!ok||!rc_sr_ok(r)||!t.random||t.motor>1||t.mode>MOTOR_DOWN_MODE||t.next_voice>=RC_DRIVESND_VOICES||
       t.level<0||t.level>4||t.speed<0||t.speed>1||t.coast<0||t.coast>1||t.turn<0||t.turn>=DRIVESND_LOOP)return 0;
    if((t.mode==MOTOR_UP_MODE&&t.age>=MOTOR_UP_LENGTH)||(t.mode==MOTOR_DOWN_MODE&&t.age>=MOTOR_DOWN_LENGTH+SETTLE)||
       ((t.mode==MOTOR_OFF||t.mode==MOTOR_RUN_MODE)&&t.age))return 0;
    if((t.mode==MOTOR_UP_MODE||t.mode==MOTOR_RUN_MODE)!=!!t.motor)return 0;
    *s=t;return 1;
}
