#include "pla.h"
#include <stdio.h>
#define CHECK(x) do{if(!(x)){fprintf(stderr,"line %d: %s\n",__LINE__,#x);return 1;}}while(0)
int main(void){rc_pla p;rc_pla_init(&p,NULL);const uint16_t common=RC_PLA_LORAM|RC_PLA_HIRAM|RC_PLA_CHAREN|RC_PLA_GAME|RC_PLA_EXROM|RC_PLA_R__W|RC_PLA_BA;
    p.inputs=common|RC_PLA_A15|RC_PLA_A13;CHECK(rc_pla_decode(p.inputs)==RC_PLA_BASIC);
    CHECK(rc_pla_decode(p.inputs&~RC_PLA_R__W)==RC_PLA_CASRAM);
    const uint16_t ultimax=(uint16_t)((common&~RC_PLA_GAME)|RC_PLA_A15|RC_PLA_A14|RC_PLA_A13);CHECK(rc_pla_decode(ultimax)==RC_PLA_ROMH);
    rc_state_writer w;rc_sw_init(&w);rc_sw_header(&w,"test","pla",NULL,0);rc_sw_begin(&w,"PLA ",0);rc_pla_save(&p,&w);rc_sw_end(&w);CHECK(rc_sw_finish(&w));p.inputs=0;
    rc_state_reader r;char error[128];CHECK(rc_sr_open(&r,w.data,w.size,error,sizeof error)&&rc_sr_chunk(&r,"PLA ",0)&&rc_pla_load(&p,&r));CHECK(rc_pla_decode(p.inputs)==RC_PLA_BASIC);rc_sw_free(&w);puts("PLA mapping and state OK");return 0;}
