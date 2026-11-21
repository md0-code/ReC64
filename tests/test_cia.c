/* Lorenz's published cascaded timer sequence, then an in-flight round trip. */
#include "cia6526.h"
#include <stdio.h>
#include <string.h>
#define CHECK(x) do{if(!(x)){fprintf(stderr,"line %d: %s\n",__LINE__,#x);return 1;}}while(0)
int main(void){
    rc_cia6526 c;rc_cia6526_init(&c,NULL,RC_CIA_6526);
    c.timer[0].counter=c.timer[0].latch=c.timer[1].counter=c.timer[1].latch=2;
    c.timer[0].control=3;c.timer[0].count_pipe=7;c.timer[1].control=0x47;c.timer[1].toggle=1;c.mask=2;
    static const uint8_t TA[]={1,2,2,1,2,2,1,2,2,1,2,2},TB[]={2,2,2,1,1,1,0,0,2,2,2,2};
    static const uint8_t PB[]={0x80,0xc0,0x80,0x80,0xc0,0x80,0x80,0xc0,0,0,0x40,0},ICR[]={0,1,1,1,1,1,1,1,3,0x83,0x83,0x83};
    for(unsigned i=0;i<12;++i){rc_cia6526_tick(&c);CHECK(c.timer[0].counter==TA[i]&&c.timer[1].counter==TB[i]);CHECK((rc_cia6526_port_output(&c,1)&0xc0)==PB[i]);CHECK((c.icr|(c.irq?128:0))==ICR[i]);}
    rc_state_writer w;rc_sw_init(&w);rc_sw_header(&w,"test","cia",NULL,0);rc_sw_begin(&w,"CIA ",0);rc_cia6526_save(&c,&w);rc_sw_end(&w);CHECK(rc_sw_finish(&w));
    rc_cia6526_tick(&c);const uint16_t a=c.timer[0].counter,b=c.timer[1].counter;
    rc_state_reader r;char error[128];CHECK(rc_sr_open(&r,w.data,w.size,error,sizeof error)&&rc_sr_chunk(&r,"CIA ",0));CHECK(rc_cia6526_load(&c,&r));rc_cia6526_tick(&c);CHECK(c.timer[0].counter==a&&c.timer[1].counter==b);rc_sw_free(&w);
    rc_cia6526_reset(&c);rc_cia6526_write(&c,15,128);rc_cia6526_write(&c,11,1);rc_cia6526_write(&c,8,1);rc_cia6526_write(&c,15,0);rc_cia6526_write(&c,11,1);rc_cia6526_write(&c,8,0);
    for(unsigned i=0;i<6;++i)rc_cia6526_tod_pulse(&c);CHECK(rc_cia6526_read(&c,8)==1&&(c.icr&4));
    const uint8_t byte=0xa5;for(unsigned i=0;i<8;++i){rc_cia6526_input(&c,RC_CIA_CNT,0);rc_cia6526_input(&c,RC_CIA_SP,(byte>>(7-i))&1u);rc_cia6526_input(&c,RC_CIA_CNT,1);}CHECK(rc_cia6526_read(&c,12)==byte&&(c.icr&8));
    puts("CIA timing, TOD, serial and state OK");return 0;
}
