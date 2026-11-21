/* Datasheet Figures 9/16/19 and SR serial transfer, with an in-flight state. */
#include "via6522.h"
#include <stdio.h>
#include <string.h>
#define CHECK(x) do{if(!(x)){fprintf(stderr,"line %d: %s\n",__LINE__,#x);return 1;}}while(0)
static void tick(rc_via6522 *v,unsigned n){while(n--)rc_via6522_tick(v);}
static void snapshot(rc_via6522 *v,rc_state_writer *w){rc_sw_init(w);rc_sw_header(w,"test","via",NULL,0);rc_sw_begin(w,"VIA ",0);rc_via6522_save(v,w);rc_sw_end(w);rc_sw_finish(w);}
int main(void) {
    rc_via6522 v;rc_via6522_init(&v,NULL);
    rc_via6522_write(&v,2,128);rc_via6522_write(&v,11,0xc0);rc_via6522_write(&v,14,0xc0);
    rc_via6522_write(&v,4,2);rc_via6522_write(&v,5,0);
    CHECK(!(rc_via6522_port_output(&v,1)&128u));
    tick(&v,3);CHECK(!v.irq&&v.t1==0);tick(&v,1);
    CHECK(v.irq&&(rc_via6522_read(&v,13)&0xc0)==0xc0&&(rc_via6522_port_output(&v,1)&128u));
    rc_via6522_read(&v,4);CHECK(!v.irq);tick(&v,4);CHECK(v.irq&&!(rc_via6522_port_output(&v,1)&128u));
    rc_via6522_write(&v,14,64);CHECK(!v.irq&&rc_via6522_read(&v,14)==128);

    rc_via6522_reset(&v);rc_via6522_write(&v,12,8); /* CA2 handshake. */
    rc_via6522_write(&v,11,1);rc_via6522_port_input(&v,0,0x5a);
    rc_via6522_input(&v,RC_VIA_CA1,0);rc_via6522_port_input(&v,0,0xa5);
    CHECK(rc_via6522_read(&v,15)==0x5a&&(v.ifr&2));
    CHECK(rc_via6522_read(&v,1)==0x5a&&!(v.ifr&2)&&rc_via6522_control_output(&v,RC_VIA_CA2)==0);
    rc_via6522_input(&v,RC_VIA_CA1,1);rc_via6522_input(&v,RC_VIA_CA1,0);
    CHECK(rc_via6522_control_output(&v,RC_VIA_CA2)==1);
    rc_via6522_write(&v,12,0xa2); /* CB2 pulse, CA2 independent falling input. */
    rc_via6522_input(&v,RC_VIA_CA2,0);rc_via6522_read(&v,1);CHECK(v.ifr&1);
    rc_via6522_read(&v,0);CHECK(rc_via6522_control_output(&v,RC_VIA_CB2)==1);
    rc_via6522_write(&v,0,0);CHECK(rc_via6522_control_output(&v,RC_VIA_CB2)==0);
    tick(&v,1);CHECK(rc_via6522_control_output(&v,RC_VIA_CB2)==1);

    rc_via6522_reset(&v);rc_via6522_write(&v,11,32);rc_via6522_write(&v,8,1);rc_via6522_write(&v,9,0);
    tick(&v,1);rc_via6522_port_input(&v,1,0xbf);tick(&v,1);CHECK(v.t2==0&&!(v.ifr&32));
    rc_via6522_port_input(&v,1,255);tick(&v,1);rc_via6522_port_input(&v,1,0xbf);tick(&v,1);CHECK(v.ifr&32);
    rc_via6522_read(&v,8);tick(&v,1);CHECK(!(v.ifr&32));

    rc_via6522_reset(&v);rc_via6522_write(&v,11,12);rc_via6522_write(&v,10,0);
    for(unsigned i=0;i<8;++i){rc_via6522_input(&v,RC_VIA_CB1,0);rc_via6522_input(&v,RC_VIA_CB2,(0xa5u>>(7u-i))&1u);rc_via6522_input(&v,RC_VIA_CB1,1);tick(&v,1);}
    CHECK(v.sr==0xa5&&(v.ifr&4));
    rc_via6522_write(&v,11,24);rc_via6522_write(&v,10,0x96);
    for(unsigned i=0;i<8;++i){tick(&v,1);CHECK(rc_via6522_control_output(&v,RC_VIA_CB2)==(int)((0x96u>>(7u-i))&1u));tick(&v,1);}
    CHECK((v.ifr&4)&&!v.sr_active&&v.sr_clock==1);

    rc_via6522_write(&v,11,0xd4);rc_via6522_write(&v,8,2);rc_via6522_write(&v,10,0x3c);tick(&v,3);
    rc_state_writer saved,expected,actual;snapshot(&v,&saved);tick(&v,37);snapshot(&v,&expected);
    rc_state_reader r;char error[128];CHECK(rc_sr_open(&r,saved.data,saved.size,error,sizeof error)&&rc_sr_chunk(&r,"VIA ",0));
    CHECK(rc_via6522_load(&v,&r));tick(&v,37);snapshot(&v,&actual);
    CHECK(!saved.failed&&!expected.failed&&!actual.failed&&actual.size==expected.size&&!memcmp(actual.data,expected.data,actual.size));
    rc_sw_free(&saved);rc_sw_free(&expected);rc_sw_free(&actual);
    puts("VIA timers, handshakes, serial and state OK");return 0;
}
