/* One foundation smoke: exact cycle order, config and a saved machine. */
#include "machine.h"
#include "sched.h"
#include <stdio.h>
#include <string.h>
#define CHECK(x) do { if(!(x)) { fprintf(stderr,"line %d: %s\n",__LINE__,#x);return 1; } } while(0)
typedef struct timer { rc_dev dev; unsigned *order; } timer;
static void event(void *self,uint64_t t) { timer *v=self;(void)t;*v->order=*v->order*10u+1u; }
static void phi1(void *self,uint64_t t) { unsigned *v=self;(void)t;*v=*v*10u+2u; }
static void phi2(void *self,uint64_t t) { unsigned *v=self;(void)t;*v=*v*10u+3u; }
static void tick(void *self,uint64_t t) { unsigned *v=self;(void)t;*v=*v*10u+4u; }
int main(void) {
    unsigned order=0;const rc_dev_ops ops={.event=event};timer v={{&ops,NULL,"test"},&order};
    rc_sched sched;rc_sched_init(&sched);CHECK(rc_sched_set(&sched,&v.dev,0));
    const rc_cycle_host board={&order,phi1,phi2,tick};
    CHECK(rc_sched_cycles(&sched,&board,1));CHECK(order==1234&&sched.now==1);
    rc_config config;char error[256];const char *yaml="machine: {model: test}\nui: {language: en}\n";
    CHECK(rc_config_parse(yaml,strlen(yaml),"rec64.yaml",&config,error,sizeof error));
    rc_machine *m=rc_machine_create(&config,NULL,error,sizeof error);CHECK(m);
    CHECK(rc_machine_poke(m,0x400,0x42));rc_machine_key(m,0,1);rc_machine_key(m,0,0);
    CHECK(rc_machine_run(m,123)==RC_RUN_OK);
    rc_state_writer w;rc_sw_init(&w);rc_machine_save(m,&w);CHECK(rc_sw_finish(&w));
    CHECK(!memcmp(w.data,"REC64ST1",8));
    rc_machine_reset(m,1);rc_state_reader r;CHECK(rc_sr_open(&r,w.data,w.size,error,sizeof error));
    CHECK(rc_machine_load(m,&r,error,sizeof error));uint8_t value=0;
    CHECK(rc_machine_time(m)==123&&rc_machine_peek(m,0x400,&value)&&value==0x42);
    char text[2048];rc_machine_text(m,text,sizeof text);CHECK(strstr(text,"A\n"));
    rc_frame f;CHECK(rc_machine_frame(m,&f)&&f.width==384&&f.height==272);
    rc_sw_free(&w);rc_machine_destroy(m);puts("Foundation OK");return 0;
}
