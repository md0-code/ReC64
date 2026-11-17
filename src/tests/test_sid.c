#include "sid.h"
#include <stdio.h>
#include <string.h>
#define CHECK(x) do{if(!(x)){fprintf(stderr,"line %d: %s\n",__LINE__,#x);return 1;}}while(0)
int main(void){
    for(unsigned model=0;model<2;++model){rc_sid s;CHECK(rc_sid_init(&s,NULL,model,model?1022727:985248,48000));
        rc_sid_write(&s,0,0x34);rc_sid_write(&s,1,0x12);rc_sid_write(&s,5,0x00);rc_sid_write(&s,6,0xf0);rc_sid_write(&s,4,0x21);rc_sid_write(&s,24,15);
        for(unsigned cycle=0;cycle<50000;++cycle)CHECK(rc_sid_clock(&s,1));int16_t buffer[4096];size_t n=rc_sid_audio_pull(&s,buffer,4096);int loud=0;for(size_t i=0;i<n;++i)if(buffer[i])loud=1;CHECK(loud);
        rc_sid_write(&s,0,0x17);CHECK(rc_sid_clock(&s,1));
        rc_state_writer w;rc_sw_init(&w);rc_sw_header(&w,"test","sid",NULL,0);rc_sw_begin(&w,"SID ",0);rc_sid_save(&s,&w);rc_sw_end(&w);CHECK(rc_sw_finish(&w));
        CHECK(rc_sid_peek(&s,0)==0x17);(void)rc_sid_peek(&s,28);CHECK(rc_sid_read(&s,0)==0x17);
        rc_state_reader probe;char probe_error[128];CHECK(rc_sr_open(&probe,w.data,w.size,probe_error,sizeof probe_error)&&rc_sr_chunk(&probe,"SID ",0)&&rc_sid_load(&s,&probe));
        CHECK(rc_sid_read(&s,0)==0x17);rc_sw_free(&w);rc_sw_init(&w);rc_sw_header(&w,"test","sid",NULL,0);rc_sw_begin(&w,"SID ",0);rc_sid_save(&s,&w);rc_sw_end(&w);CHECK(rc_sw_finish(&w));
        CHECK(rc_sid_clock(&s,4096));int16_t expected[512],actual[512];n=rc_sid_audio_pull(&s,expected,512);
        rc_state_reader r;char error[128];CHECK(rc_sr_open(&r,w.data,w.size,error,sizeof error)&&rc_sr_chunk(&r,"SID ",0)&&rc_sid_load(&s,&r));
        CHECK(rc_sid_clock(&s,4096));CHECK(rc_sid_audio_pull(&s,actual,512)==n&&!memcmp(expected,actual,n*sizeof *actual));CHECK(rc_sid_ok(&s));rc_sw_free(&w);rc_sid_release(&s);
    }puts("6581/8580 audio, resampling and exact state OK");return 0;
}
