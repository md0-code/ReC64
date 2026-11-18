#include "palette.h"
#include <stdio.h>
#include <string.h>
#define CHECK(x) do{if(!(x)){fprintf(stderr,"line %d: %s\n",__LINE__,#x);return 1;}}while(0)
int main(void){
    const uint32_t *standard=rc_palette_builtin("default"),*corrected=rc_palette_builtin("color-corrected");
    CHECK(standard&&standard[0]==0&&standard[1]==0xffffff&&standard[6]==0x342779&&standard[12]==0x6b6b6b);
    CHECK(corrected&&corrected[1]==0xffffff&&corrected[6]==0x1f20aa&&corrected[12]==0x636363&&!rc_palette_builtin("other"));
    char error[128];uint32_t colors[16];
    rc_state_writer w;rc_sw_init(&w);rc_sw_header(&w,"test","palette",NULL,0);rc_sw_begin(&w,"PAL ",0);rc_palette_save(corrected,&w);rc_sw_end(&w);CHECK(rc_sw_finish(&w));
    rc_state_reader r;CHECK(rc_sr_open(&r,w.data,w.size,error,sizeof error)&&rc_sr_chunk(&r,"PAL ",0)&&rc_palette_load(colors,&r)&&!memcmp(colors,corrected,sizeof colors));
    rc_sw_free(&w);puts("Computed palettes and state OK");return 0;
}
