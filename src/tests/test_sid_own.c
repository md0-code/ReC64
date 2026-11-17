#include "sid_own.h"
#include <stdio.h>
#include <string.h>
#define CHECK(x) do {if(!(x)){fprintf(stderr,"line %d: %s\n",__LINE__,#x);return 1;}} while(0)
static void wr(rc_sid_own *s,unsigned r,uint8_t v){rc_sid_own_write(s,r,v);rc_sid_own_tick(s);}
int main(void) {
    rc_sid_own s,restored;rc_state_writer w;rc_state_reader r;char error[128];
    int16_t expected[512],actual[512];
    CHECK(rc_sid_own_init(&s,RC_SID_OWN_8580,985248,48000));
    wr(&s,0,0x34);wr(&s,1,0x12);wr(&s,5,0);wr(&s,6,0xf0);wr(&s,4,0x21);
    wr(&s,21,7);wr(&s,22,100);wr(&s,23,0x51);wr(&s,24,0x1f);
    CHECK(rc_sid_own_clock(&s,4000));
    CHECK(s.voice[0].envelope==255 && s.voice[0].waveform>0);
    CHECK(rc_sid_own_audio_pull(&s,actual,512)>100);
    /* Snapshot deliberately lands with an HMOS write pending and filter energy. */
    rc_sid_own_write(&s,4,0x80);
    rc_sw_init(&w);rc_sw_header(&w,"test","sid-own",NULL,0);
    rc_sw_begin(&w,"SIDO",0);rc_sid_own_save(&s,&w);rc_sw_end(&w);CHECK(rc_sw_finish(&w));
    CHECK(rc_sid_own_clock(&s,4096));size_t count=rc_sid_own_audio_pull(&s,expected,512);
    CHECK(rc_sid_own_init(&restored,RC_SID_OWN_8580,985248,48000));
    CHECK(rc_sr_open(&r,w.data,w.size,error,sizeof error) && rc_sr_chunk(&r,"SIDO",0));
    CHECK(rc_sid_own_load(&restored,&r) && rc_sid_own_clock(&restored,4096));
    CHECK(rc_sid_own_audio_pull(&restored,actual,512)==count);
    CHECK(!memcmp(actual,expected,count*sizeof *actual));
    CHECK(!memcmp(&s,&restored,sizeof s));
    int loud=0;for(size_t i=0;i<count;++i)if(actual[i])loud=1;CHECK(loud);
    rc_sw_free(&w);
    puts("own SID native audio and exact pending-write/filter/resampler state OK");return 0;
}
