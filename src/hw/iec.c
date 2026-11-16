#include "iec.h"
void rc_iec_init(rc_iec *b){b->output[0]=b->output[1]=b->lines=RC_IEC_ALL;b->edges=0;}
void rc_iec_output(rc_iec *b,unsigned endpoint,unsigned released){if(endpoint>1)return;b->output[endpoint]=(uint8_t)(released&RC_IEC_ALL);const uint8_t next=b->output[0]&b->output[1];if(next!=b->lines){++b->edges;b->lines=next;}}
unsigned rc_iec_lines(const rc_iec *b){return b->lines;}
void rc_iec_save(const rc_iec *b,rc_state_writer *w){rc_sw_u32(w,1);rc_sw_bytes(w,b->output,2);rc_sw_u8(w,b->lines);rc_sw_u64(w,b->edges);}
int rc_iec_load(rc_iec *b,rc_state_reader *r){if(rc_sr_u32(r)!=1)return 0;rc_iec n;rc_sr_bytes(r,n.output,2);n.lines=rc_sr_u8(r);n.edges=rc_sr_u64(r);if(!rc_sr_ok(r)||n.lines!=(n.output[0]&n.output[1])||(n.lines&~RC_IEC_ALL))return 0;*b=n;return 1;}
