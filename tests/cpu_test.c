#include "mos6510.h"
#include <stdio.h>
#define CHECK(x) do{if(!(x)){fprintf(stderr,"line %d: %s\n",__LINE__,#x);return 1;}}while(0)
int main(void) {
    rc_mos6510 c;rc_mos6510_init(&c,NULL,RC_MOS6510);c.resetting=0;c.pc=0x800;c.a=1;rc_mos6510_start(&c);
    rc_mos6510_clock(&c,0x69,1);CHECK(c.pins.address==0x801);
    rc_mos6510_clock(&c,0xff,0);CHECK(c.pc==0x801);
    rc_mos6510_input(&c,RC_CPU_NMI,1);
    rc_state_writer w;rc_sw_init(&w);rc_sw_header(&w,"test","6510",NULL,0);rc_sw_begin(&w,"CPU ",0);rc_mos6510_save(&c,&w);rc_sw_end(&w);CHECK(rc_sw_finish(&w));
    rc_mos6510_clock(&c,2,1);CHECK(c.a==3);
    rc_state_reader r;char error[128];CHECK(rc_sr_open(&r,w.data,w.size,error,sizeof error));CHECK(rc_sr_chunk(&r,"CPU ",0));CHECK(rc_mos6510_load(&c,&r));
    rc_mos6510_clock(&c,2,1);CHECK(c.a==3&&c.pins.sync&&c.nmi_pending&&!c.nmi_edge);
    rc_mos6510_port_input(&c,0x10);rc_mos6510_port_write(&c,0,0x0f);rc_mos6510_port_write(&c,1,0x05);CHECK(rc_mos6510_port_read(&c,1)==0x15);
    rc_mos6510_reset(&c);for(unsigned i=0;i<7;++i)rc_mos6510_clock(&c,i==5?0x34:i==6?0x12:0,1);
    CHECK(c.pc==0x1234&&c.s==0xfd&&c.pins.sync);
    rc_sw_free(&w);puts("CPU state, port, RDY and reset OK");return 0;
}
