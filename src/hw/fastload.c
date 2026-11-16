#include "fastload.h"
static uint32_t quiet_cycles(const rc_fastload *s){return s->clock_hz/50u+(s->clock_hz%50u!=0);}
void rc_fastload_reset(rc_fastload *s,uint64_t now,uint64_t edge_count){
    s->now=s->last_edge_at=now;s->last_edges=edge_count;s->motor=s->recent_edge=0;
}
void rc_fastload_init(rc_fastload *s,uint32_t clock_hz){s->clock_hz=clock_hz;rc_fastload_reset(s,0,0);}
void rc_fastload_observe(rc_fastload *s,uint64_t now,int motor,uint64_t edge_count){
    s->now=now;s->motor=(uint8_t)(motor!=0);
    if(edge_count!=s->last_edges){s->last_edges=edge_count;s->last_edge_at=now;s->recent_edge=1;}
    /* Unsigned subtraction also preserves the interval across cycle wrap. */
    if(s->recent_edge&&now-s->last_edge_at>=quiet_cycles(s))s->recent_edge=0;
}
int rc_fastload_active(const rc_fastload *s){return s->motor||s->recent_edge;}
void rc_fastload_save(const rc_fastload *s,rc_state_writer *w){
    rc_sw_u32(w,1);rc_sw_u32(w,s->clock_hz);rc_sw_u64(w,s->now);
    rc_sw_u64(w,s->last_edges);rc_sw_u64(w,s->last_edge_at);
    rc_sw_u8(w,s->motor);rc_sw_u8(w,s->recent_edge);
}
int rc_fastload_load(rc_fastload *s,rc_state_reader *r){
    if(rc_sr_u32(r)!=1)return 0;
    rc_fastload n;n.clock_hz=rc_sr_u32(r);n.now=rc_sr_u64(r);
    n.last_edges=rc_sr_u64(r);n.last_edge_at=rc_sr_u64(r);
    n.motor=rc_sr_u8(r);n.recent_edge=rc_sr_u8(r);
    if(!rc_sr_ok(r)||!n.clock_hz||n.clock_hz!=s->clock_hz||n.motor>1||n.recent_edge>1||
       (n.recent_edge&&n.now-n.last_edge_at>=quiet_cycles(&n)))return 0;
    *s=n;return 1;
}
