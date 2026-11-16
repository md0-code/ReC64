#include "palette.h"
#include <stdio.h>
#include <string.h>
#define CHECK(x) do{if(!(x)){fprintf(stderr,"line %d: %s\n",__LINE__,#x);return 1;}}while(0)
int main(void){
    const uint32_t *published=rc_palette_builtin("cjam");CHECK(published&&published[2]==0x7d202c&&published[14]==0x6351db);
    char text[512],error[128];const size_t size=rc_palette_write(published,text,sizeof text);uint32_t colors[16];
    CHECK(size&&rc_palette_parse((uint8_t *)text,size,colors,error,sizeof error)&&!memcmp(colors,published,sizeof colors));
    CHECK(!rc_palette_parse((uint8_t *)text,size-9,colors,error,sizeof error));
    rc_state_writer w;rc_sw_init(&w);rc_sw_header(&w,"test","palette",NULL,0);rc_sw_begin(&w,"VPL ",0);rc_palette_save(published,&w);rc_sw_end(&w);CHECK(rc_sw_finish(&w));
    rc_state_reader r;CHECK(rc_sr_open(&r,w.data,w.size,error,sizeof error)&&rc_sr_chunk(&r,"VPL ",0)&&rc_palette_load(colors,&r)&&!memcmp(colors,published,sizeof colors));
    rc_sw_free(&w);puts("Published palette, VPL reader/writer and state OK");return 0;
}
