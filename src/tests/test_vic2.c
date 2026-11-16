/* One headless raster: published fetch timing, graphics/sprite collision and
 * a half-cycle state round trip. The fixture is RAM, not another VIC model. */
#include "vic2.h"
#include "../platform/png.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define CHECK(x) do{if(!(x)){fprintf(stderr,"line %d: %s\n",__LINE__,#x);return 1;}}while(0)
static uint8_t ram[16384];
static void half(rc_vic2 *v){if(!v->phase)rc_vic2_begin(v);const unsigned p=v->phase;const rc_vic_access b=v->bus[p];rc_vic2_clock(v,p,b.kind==RC_VIC_NONE?255:ram[b.address],5);}
static void save(rc_vic2 *v,rc_state_writer *w){rc_sw_init(w);rc_sw_header(w,"test","vic",NULL,0);rc_sw_begin(w,"VIC2",0);rc_vic2_save(v,w);rc_sw_end(w);rc_sw_finish(w);}
int main(void){
    memset(ram+0x400,1,1000);memset(ram+0x2008,255,8);ram[0x7f8]=ram[0x7f9]=0xc0;memset(ram+0x3000,255,63);
    rc_vic2 *v=malloc(sizeof *v);CHECK(v);
    for(unsigned model=0;model<4;++model){
        rc_vic2_init(v,NULL,model);rc_vic2_write(v,17,0x1b);rc_vic2_write(v,22,8);rc_vic2_write(v,24,0x18);rc_vic2_write(v,32,6);rc_vic2_write(v,33,0);rc_vic2_write(v,18,51);rc_vic2_write(v,26,7);
        rc_vic2_write(v,0,24);rc_vic2_write(v,2,24);rc_vic2_write(v,1,50);rc_vic2_write(v,3,50);rc_vic2_write(v,21,3);rc_vic2_write(v,39,2);rc_vic2_write(v,40,3);
        unsigned matrix=0;while(v->frames<1){
            if(!v->phase){rc_vic2_begin(v);if(v->raster==51){if(v->bus[1].kind==RC_VIC_MATRIX){++matrix;CHECK(v->bus[1].address==0x400u+matrix-1);CHECK(v->aec==0);}if(v->cycle==12)CHECK(v->ba==0&&v->aec==1);}}
            half(v);
        }
        CHECK(matrix==40);CHECK(v->cycles==(uint64_t)rc_vic2_cycles_per_line(v)*rc_vic2_height(v));
        CHECK((rc_vic2_read(v,30)&3u)==3u&&(rc_vic2_read(v,31)&3u)==3u);CHECK((rc_vic2_read(v,25)&0x87u)==0x87u);
        const unsigned width=rc_vic2_width(v),at=51u*width+132u;CHECK(v->pixels[at]==2&&v->pixels[at+24]==5);
        if(model==0){
            static const uint32_t palette[16]={0x000000,0xffffff,0x880000,0xaaffee,0xcc44cc,0x00cc55,0x0000aa,0xeeee77,0xdd8855,0x664400,0xff7777,0x333333,0x777777,0xaaff66,0x0088ff,0xbbbbbb};
            uint32_t *argb=malloc(width*312u*sizeof *argb);CHECK(argb);
            for(unsigned i=0;i<width*312u;++i)argb[i]=0xff000000u|palette[v->pixels[i]&15u];
            CHECK(rc_png_write(RC_WORK "/tests/vic2-pal.png",argb,(int)width,312,width*sizeof *argb,0));free(argb);
        }
        half(v);rc_state_writer w;save(v,&w);CHECK(!w.failed);
        for(unsigned i=0;i<33;++i)half(v);rc_state_writer expected;save(v,&expected);
        rc_state_reader r;char error[128];CHECK(rc_sr_open(&r,w.data,w.size,error,sizeof error)&&rc_sr_chunk(&r,"VIC2",0)&&rc_vic2_load(v,&r));
        for(unsigned i=0;i<33;++i)half(v);rc_state_writer actual;save(v,&actual);CHECK(actual.size==expected.size&&!memcmp(actual.data,expected.data,actual.size));
        rc_sw_free(&w);rc_sw_free(&expected);rc_sw_free(&actual);
    }free(v);puts("VIC-II PAL/NTSC fetches, pixels, collisions and half-cycle state OK");return 0;
}
