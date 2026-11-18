#include "drivesnd.h"
#include "drivesnd_data.h"
#include <string.h>

/* The motor is three recordings: the start, which runs into a loop of the
 * running drive, and the stop. A change of state fades from one into the
 * other. The head has four recordings of a track change, taken in turn, and
 * one of the knock on its stop. A track change is two steps and a knock
 * spans five, so a step that follows closely is already in the recording
 * being played. The recordings are at 24 kHz and are interpolated. */
enum { MOTOR_NONE,MOTOR_UP_KIND,MOTOR_LOOP_KIND,MOTOR_DOWN_KIND };
enum { VOICE_IDLE,VOICE_STEP,VOICE_KNOCK=VOICE_STEP+4u };
#define ROWS(table) ((unsigned)(sizeof(table)/sizeof(table)[0]))
#define FADE 960u           /* 40 ms between two motor recordings */
#define STEP_PAIR 528u      /* 22 ms: the second step of a track change falls inside */
#define KNOCK_STEPS 5u      /* steps against the stop per recorded knock */
#define KNOCK_REST 2400u    /* 100 ms without a hit: the next one knocks at once */
typedef struct sound { const int16_t *data; unsigned length; } sound;
static const sound VOICE[6]={
    {0},{STEP_1,ROWS(STEP_1)},{STEP_2,ROWS(STEP_2)},{STEP_3,ROWS(STEP_3)},{STEP_4,ROWS(STEP_4)},{KNOCK,ROWS(KNOCK)}
};
_Static_assert(RC_DRIVESND_RATE==2u*DRIVESND_DATA_RATE,"the recordings are at half the output rate");
static const sound MOTOR[4]={{0},{MOTOR_UP,ROWS(MOTOR_UP)},{MOTOR_LOOP,ROWS(MOTOR_LOOP)},{MOTOR_DOWN,ROWS(MOTOR_DOWN)}};

/* One sample of a motor recording: the start runs into the loop, the loop
 * into itself, the stop into silence. */
static int source_sample(rc_drivesnd_source *p){
    if(!p->kind)return 0;
    const int v=MOTOR[p->kind].data[p->at]*(int)p->gain/256;
    if(++p->at>=MOTOR[p->kind].length){
        p->at=0;
        if(p->kind==MOTOR_UP_KIND)p->kind=MOTOR_LOOP_KIND;
        else if(p->kind==MOTOR_DOWN_KIND)*p=(rc_drivesnd_source){0};
    }
    return v;
}
static void motor_turn(rc_drivesnd *s,unsigned kind,unsigned gain){
    s->before=s->now;s->fade=s->before.kind?FADE:0;
    s->now=(rc_drivesnd_source){0,(uint16_t)gain,(uint8_t)kind};
}
static void trigger(rc_drivesnd *s,unsigned kind){
    s->voice[s->next_voice]=(rc_drivesnd_voice){0,(uint8_t)kind};
    s->next_voice=(uint8_t)((s->next_voice+1u)%RC_DRIVESND_VOICES);
}
static void reset_dev(void *self){rc_drivesnd_reset(self);}
static void save_dev(const void *self,rc_state_writer *w){rc_drivesnd_save(self,w);}
static int load_dev(void *self,rc_state_reader *r){return rc_drivesnd_load(self,r);}
static const rc_dev_ops OPS={.reset=reset_dev,.save=save_dev,.load=load_dev};
void rc_drivesnd_reset(rc_drivesnd *s){
    memset(s,0,sizeof *s);s->dev=(rc_dev){&OPS,NULL,"1541 sound"};s->step_gap=STEP_PAIR;s->knock_gap=KNOCK_REST;
}
void rc_drivesnd_init(rc_drivesnd *s){rc_drivesnd_reset(s);}
void rc_drivesnd_status(rc_drivesnd *s,int motor,uint64_t steps,uint64_t bumps){
    motor=motor!=0;
    if(motor!=s->motor){
        s->motor=(uint8_t)motor;
        if(motor){
            /* A motor still coasting picks up speed again without the start-up. */
            if(s->now.kind==MOTOR_DOWN_KIND)motor_turn(s,MOTOR_LOOP_KIND,256);
            else if(!s->now.kind)motor_turn(s,MOTOR_UP_KIND,256);
        }else if(s->now.kind==MOTOR_UP_KIND){
            /* Stopped while starting: coast down from the speed reached. */
            motor_turn(s,MOTOR_DOWN_KIND,256u*s->now.at/ROWS(MOTOR_UP));
        }else if(s->now.kind==MOTOR_LOOP_KIND)motor_turn(s,MOTOR_DOWN_KIND,s->now.gain);
    }
    uint64_t moved=steps>=s->steps?steps-s->steps:0,hits=bumps>=s->bumps?bumps-s->bumps:0;
    s->steps=steps;s->bumps=bumps;
    /* A step against the stop does not move the head: it is heard as the knock only. */
    moved=moved>hits?moved-hits:0;
    if(hits>RC_DRIVESND_VOICES)hits=RC_DRIVESND_VOICES;
    for(;hits;--hits){
        if(!s->knock_phase)trigger(s,VOICE_KNOCK);
        s->knock_phase=(uint8_t)((s->knock_phase+1u)%KNOCK_STEPS);s->knock_gap=0;
    }
    if(moved&&s->step_gap>=STEP_PAIR){trigger(s,VOICE_STEP+(s->step_turn++&3u));s->step_gap=0;}
}
/* One sample at the recordings' rate. */
static int mix(rc_drivesnd *s){
    int v=source_sample(&s->now);
    if(s->fade){
        v=(v*(int)(FADE-s->fade)+source_sample(&s->before)*(int)s->fade)/(int)FADE;
        if(!--s->fade)s->before=(rc_drivesnd_source){0};
    }
    for(unsigned j=0;j<RC_DRIVESND_VOICES;++j){
        rc_drivesnd_voice *p=&s->voice[j];if(!p->kind)continue;
        v+=VOICE[p->kind].data[p->age];
        if(++p->age>=VOICE[p->kind].length)*p=(rc_drivesnd_voice){0};
    }
    if(s->step_gap<STEP_PAIR)++s->step_gap;
    if(s->knock_gap<KNOCK_REST&&++s->knock_gap==KNOCK_REST)s->knock_phase=0;
    return v;
}
int16_t rc_drivesnd_sample(rc_drivesnd *s){
    int v;
    if(!s->half){
        memmove(s->line,s->line+1,3u*sizeof s->line[0]);s->line[3]=mix(s);
        v=s->line[1];
    }else {
        /* Halfway between two samples, from the two on either side. */
        v=9*(s->line[1]+s->line[2])-s->line[0]-s->line[3];
        v=(v>=0?v+8:v-8)/16;
    }
    s->half^=1u;
    if(v>32767)v=32767;else if(v<-32768)v=-32768;
    return (int16_t)v;
}
static void save_source(const rc_drivesnd_source *p,rc_state_writer *w){rc_sw_u8(w,p->kind);rc_sw_u32(w,p->at);rc_sw_u16(w,p->gain);}
static int load_source(rc_drivesnd_source *p,rc_state_reader *r){
    p->kind=rc_sr_u8(r);p->at=rc_sr_u32(r);p->gain=rc_sr_u16(r);
    return p->kind<=MOTOR_DOWN_KIND&&p->gain<=256u&&(p->kind?p->at<MOTOR[p->kind].length:!p->at&&!p->gain);
}
void rc_drivesnd_save(const rc_drivesnd *s,rc_state_writer *w){
    rc_sw_u32(w,5);rc_sw_u64(w,s->steps);rc_sw_u64(w,s->bumps);
    rc_sw_u8(w,s->motor);rc_sw_u8(w,s->next_voice);rc_sw_u8(w,s->step_turn);rc_sw_u8(w,s->knock_phase);rc_sw_u8(w,s->half);
    save_source(&s->now,w);save_source(&s->before,w);
    rc_sw_u32(w,s->fade);rc_sw_u32(w,s->step_gap);rc_sw_u32(w,s->knock_gap);
    for(unsigned i=0;i<4;++i)rc_sw_u32(w,(uint32_t)s->line[i]);
    for(unsigned j=0;j<RC_DRIVESND_VOICES;++j){rc_sw_u8(w,s->voice[j].kind);rc_sw_u32(w,s->voice[j].age);}
}
int rc_drivesnd_load(rc_drivesnd *s,rc_state_reader *r){
    rc_drivesnd t;rc_drivesnd_init(&t);
    const uint32_t payload=rc_sr_u32(r);
    t.steps=rc_sr_u64(r);t.bumps=rc_sr_u64(r);
    if(payload==1||payload==3||payload==4){
        /* The synthesised sounds of earlier versions: keep the mechanism
         * counters and the motor, and let the sound start afresh. */
        if(payload==1){
            for(unsigned i=0;i<8;++i)(void)rc_sr_u32(r);
            t.motor=rc_sr_u8(r);(void)rc_sr_u8(r);(void)rc_sr_u8(r);
            for(unsigned j=0;j<8;++j){for(unsigned i=0;i<5;++i)(void)rc_sr_u32(r);(void)rc_sr_u8(r);}
        }else if(payload==3){
            (void)rc_sr_u32(r);(void)rc_sr_u32(r);
            t.motor=rc_sr_u8(r);(void)rc_sr_u8(r);(void)rc_sr_u8(r);(void)rc_sr_u8(r);
            for(unsigned i=0;i<11;++i)(void)rc_sr_u64(r);
            for(unsigned j=0;j<9;++j){(void)rc_sr_u8(r);(void)rc_sr_u32(r);for(unsigned i=0;i<66;++i)(void)rc_sr_u64(r);}
        }else {
            (void)rc_sr_u32(r);(void)rc_sr_u32(r);
            t.motor=rc_sr_u8(r);(void)rc_sr_u8(r);(void)rc_sr_u8(r);
            for(unsigned i=0;i<24;++i)(void)rc_sr_u64(r);
            for(unsigned j=0;j<8;++j){(void)rc_sr_u8(r);(void)rc_sr_u32(r);}
        }
        if(!rc_sr_ok(r)||t.motor>1)return 0;
        if(t.motor)t.now=(rc_drivesnd_source){0,256,MOTOR_LOOP_KIND};
        *s=t;return 1;
    }
    if(payload!=5)return 0;
    t.motor=rc_sr_u8(r);t.next_voice=rc_sr_u8(r);t.step_turn=rc_sr_u8(r);t.knock_phase=rc_sr_u8(r);t.half=rc_sr_u8(r);
    int ok=load_source(&t.now,r);ok&=load_source(&t.before,r);
    t.fade=rc_sr_u32(r);t.step_gap=rc_sr_u32(r);t.knock_gap=rc_sr_u32(r);
    for(unsigned i=0;i<4;++i)t.line[i]=(int32_t)rc_sr_u32(r);
    for(unsigned j=0;j<RC_DRIVESND_VOICES;++j){
        rc_drivesnd_voice *p=&t.voice[j];p->kind=rc_sr_u8(r);p->age=rc_sr_u32(r);
        if(p->kind>VOICE_KNOCK||(p->kind?p->age>=VOICE[p->kind].length:p->age!=0))ok=0;
    }
    if(!ok||!rc_sr_ok(r)||t.motor>1||t.next_voice>=RC_DRIVESND_VOICES||t.knock_phase>=KNOCK_STEPS||t.half>1||
       t.fade>FADE||t.step_gap>STEP_PAIR||t.knock_gap>KNOCK_REST||(!t.fade&&t.before.kind))return 0;
    /* A motor that is on is starting or running; one that is off coasts or stands. */
    if((t.now.kind==MOTOR_UP_KIND||t.now.kind==MOTOR_LOOP_KIND)!=!!t.motor)return 0;
    for(unsigned i=0;i<4;++i)if(t.line[i]>16*32768||t.line[i]<-16*32768)return 0;
    *s=t;return 1;
}
