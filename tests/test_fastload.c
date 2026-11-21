#include "fastload.h"
#include <stdio.h>
#include <string.h>
#define CHECK(x) do{if(!(x)){fprintf(stderr,"line %d: %s\n",__LINE__,#x);return 1;}}while(0)
static void save(const rc_fastload *s,rc_state_writer *w){
    rc_sw_init(w);rc_sw_header(w,"test","fastload",NULL,0);rc_sw_begin(w,"FAST",0);
    rc_fastload_save(s,w);rc_sw_end(w);rc_sw_finish(w);
}
int main(void){
    rc_fastload s;rc_fastload_init(&s,985248);CHECK(!rc_fastload_active(&s));
    rc_fastload_reset(&s,100,7);rc_fastload_observe(&s,101,1,7);CHECK(rc_fastload_active(&s));
    /* Motor-only activity ends immediately; steady low IEC levels add no tail. */
    rc_fastload_observe(&s,102,0,7);CHECK(!rc_fastload_active(&s));
    rc_fastload_observe(&s,103,0,8);CHECK(rc_fastload_active(&s));
    const uint64_t quiet=19705;rc_fastload_observe(&s,103+quiet-1,0,8);CHECK(rc_fastload_active(&s));
    rc_state_writer before,expected,actual;save(&s,&before);CHECK(!before.failed);
    rc_fastload_observe(&s,103+quiet,0,8);CHECK(!rc_fastload_active(&s));save(&s,&expected);
    char error[128];rc_state_reader r;CHECK(rc_sr_open(&r,before.data,before.size,error,sizeof error)&&rc_sr_chunk(&r,"FAST",0)&&rc_fastload_load(&s,&r));
    CHECK(rc_fastload_active(&s));rc_fastload_observe(&s,103+quiet,0,8);save(&s,&actual);
    CHECK(!expected.failed&&!actual.failed&&expected.size==actual.size&&!memcmp(expected.data,actual.data,actual.size));
    rc_sw_free(&before);rc_sw_free(&expected);rc_sw_free(&actual);
    puts("Fast loading motor/serial activity, idle return and exact state OK");return 0;
}
