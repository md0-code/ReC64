#include "palette.h"
#include <stdio.h>
#include <string.h>
#define CHECK(x) do{if(!(x)){fprintf(stderr,"line %d: %s\n",__LINE__,#x);return 1;}}while(0)
int main(void){
    const uint32_t *published=rc_palette_profile("color-corrected",0);CHECK(published&&published[2]==0x7d202c&&published[14]==0x6351db);
    CHECK(memcmp(rc_palette_profile("default",0),rc_palette_profile("default",1),16*sizeof(uint32_t)));
    CHECK(rc_palette_profile("color-corrected",0)==rc_palette_profile("color-corrected",1));
    const uint32_t *tuned=rc_palette_profile("tuned",1);CHECK(tuned&&tuned[0]==0&&tuned[1]==0xffffff&&tuned[6]==0x20229c&&tuned[12]==0x636363);
    char error[128];uint32_t colors[16];
    rc_state_writer w;rc_sw_init(&w);rc_sw_header(&w,"test","palette",NULL,0);rc_sw_begin(&w,"VPL ",0);rc_palette_save(published,&w);rc_sw_end(&w);CHECK(rc_sw_finish(&w));
    rc_state_reader r;CHECK(rc_sr_open(&r,w.data,w.size,error,sizeof error)&&rc_sr_chunk(&r,"VPL ",0)&&rc_palette_load(colors,&r)&&!memcmp(colors,published,sizeof colors));
    rc_sw_free(&w);puts("Built-in color profiles, automatic PAL/NTSC and state OK");return 0;
}
