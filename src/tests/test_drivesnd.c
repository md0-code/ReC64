#include "../hw/drivesnd.h"
#include <stdio.h>
#include <string.h>
#define CHECK(x) do{if(!(x)){fprintf(stderr,"line %d: %s\n",__LINE__,#x);return 1;}}while(0)
static uint64_t energy(rc_drivesnd *s,unsigned n){
    uint64_t sum=0;while(n--){int32_t v=rc_drivesnd_sample(s);sum+=(uint64_t)((int64_t)v*v);}return sum;
}
static int snapshot(const rc_drivesnd *s,rc_state_writer *w){
    rc_sw_init(w);rc_sw_header(w,"drive-sound","1541",NULL,0);
    rc_sw_begin(w,"DSND",0);rc_drivesnd_save(s,w);rc_sw_end(w);return rc_sw_finish(w);
}
int main(void){
    rc_drivesnd s,t;rc_drivesnd_init(&s);CHECK(energy(&s,480)==0);
    rc_drivesnd_status(&s,1,0,0);CHECK(energy(&s,24000)>0&&s.now.kind==2);
    uint64_t hum=energy(&s,4800);CHECK(hum>4800u*100u*100u&&hum<4800ull*2000u*2000u);
    rc_drivesnd_reset(&s);rc_drivesnd_status(&s,0,1,0);uint64_t step=energy(&s,6000);CHECK(step>0&&energy(&s,64)==0);
    rc_drivesnd_status(&s,0,1,0);CHECK(energy(&s,64)==0);
    rc_drivesnd_status(&s,0,1,1);uint64_t bump=energy(&s,7000);CHECK(bump>step&&energy(&s,64)==0);
    rc_drivesnd_status(&s,0,0,0);CHECK(energy(&s,64)==0);
    /* Snapshot during overlapping steps/bump and motor attack. */
    rc_drivesnd_status(&s,1,3,1);energy(&s,517);rc_drivesnd_status(&s,1,4,2);energy(&s,137);
    rc_state_writer a,b,c;CHECK(snapshot(&s,&a));rc_drivesnd_init(&t);
    rc_state_reader r;char error[128];CHECK(rc_sr_open(&r,a.data,a.size,error,sizeof error)&&rc_sr_chunk(&r,"DSND",0)&&rc_drivesnd_load(&t,&r));
    /* The entire future waveform and serialized state must match bit for bit. */
    for(unsigned i=0;i<24000;++i){
        if(i==300){rc_drivesnd_status(&s,0,4,2);rc_drivesnd_status(&t,0,4,2);}
        if(i==777){rc_drivesnd_status(&s,1,5,2);rc_drivesnd_status(&t,1,5,2);}
        CHECK(rc_drivesnd_sample(&s)==rc_drivesnd_sample(&t));
    }
    CHECK(snapshot(&s,&b)&&snapshot(&t,&c));CHECK(b.size==c.size&&!memcmp(b.data,c.data,b.size));
    rc_sw_free(&a);rc_sw_free(&b);rc_sw_free(&c);
    rc_drivesnd_status(&s,0,5,2);CHECK(energy(&s,20000)>0&&s.now.kind==0&&energy(&s,100)==0);
    /* Malformed state is rejected atomically. */
    CHECK(snapshot(&s,&a));CHECK(rc_sr_open(&r,a.data,a.size,error,sizeof error)&&rc_sr_chunk(&r,"DSND",0));
    a.data[r.at]=2;CHECK(!rc_drivesnd_load(&s,&r));rc_sw_free(&a);
    printf("1541 recorded motor, steps and knock with exact 48kHz state passed (hum=%llu step=%llu bump=%llu)\n",(unsigned long long)hum,(unsigned long long)step,(unsigned long long)bump);return 0;
}
