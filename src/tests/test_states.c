#include "../app/states.h"
#include "../app/pace.h"
#include "../core/yamledit.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#define CHECK(x) do{if(!(x)){fprintf(stderr,"line %d: %s\n",__LINE__,#x);return 1;}}while(0)
int main(void){
    rc_pace p;rc_pace_init(&p,1000000,985248,100);rc_pace_reset(&p,5000,0);uint64_t guest=0;
    for(unsigned i=0;i<30;++i){const rc_pace_step s=rc_pace_next(&p,1,5000+i,guest,19656);if(s.action==RC_PACE_RUN)guest+=s.ticks;}
    rc_pace_next(&p,0,6000,guest,19656);CHECK(rc_pace_due(&p,6000)==guest);
    const rc_pace_step resumed=rc_pace_next(&p,0,8000,guest,19656);CHECK(resumed.action==RC_PACE_RUN&&resumed.ticks>0&&resumed.ticks<=p.slice);
    rc_config c;rc_config_defaults(&c);snprintf(c.model,sizeof c.model,"test");char error[256];rc_machine *m=rc_machine_create(&c,NULL,error,sizeof error);CHECK(m);
    rc_states_info info={0},read;rc_frame frame;CHECK(rc_machine_frame(m,&frame));info.has_thumb=rc_states_thumb_make(&frame,&info.thumb);CHECK(info.has_thumb);snprintf(info.version,sizeof info.version,"test");
    rc_state_writer w;rc_sw_init(&w);CHECK(rc_states_write(m,&info,&w)&&rc_states_finish(&w));rc_state_reader r;CHECK(rc_states_read(w.data,w.size,&read,&r,error,sizeof error)&&read.has_thumb);
    CHECK(rc_sr_chunk(&r,"VERS",0));w.data[r.at+2]^=1;CHECK(!rc_states_read(w.data,w.size,&read,&r,error,sizeof error)&&strstr(error,"checksum"));
    rc_sw_free(&w);rc_machine_destroy(m);
    const char *flow="# Keep this comment\n{\"machine\": {\"model\": \"test\"}, \"ui\": {\"setup_page\": \"states\"}}\n";
    rc_ydoc doc;CHECK(rc_ydoc_parse(&doc,flow,strlen(flow)));c.state_slot=10;CHECK(rc_config_store(&c,&doc));size_t length;char *updated=rc_ydoc_text(&doc,&length);CHECK(updated&&strstr(updated,"# Keep this comment"));rc_config loaded;CHECK(rc_config_parse(updated,length,"test.yaml",&loaded,error,sizeof error)&&loaded.state_slot==10);free(updated);rc_ydoc_free(&doc);
    puts("Snapshot preview/integrity, slot settings and full-to-normal pacing passed");return 0;
}
