/* One headless touch/controller flow against the actual C64 switch matrix. */
#include "../src/app/osk.h"
#include "../src/hw/c64kbd.h"
#include "../src/platform/png.h"
#include <stdio.h>
#include <string.h>
#define CHECK(x) do{if(!(x)){fprintf(stderr,"line %d: %s\n",__LINE__,#x);return 1;}}while(0)
static void set_key(void *ctx,unsigned key,int down){rc_c64kbd_key(ctx,key,down);}
static void center(const rc_osk *o,unsigned key,int *x,int *y){
    unsigned count;const rc_c64_keycap *caps=rc_c64_layout(&count);
    for(unsigned i=0;i<count;++i)if(caps[i].key==key){
        *x=o->left+(2*caps[i].x+caps[i].w)*o->width/160;
        *y=o->top+16+(2*caps[i].y+caps[i].h)*(o->height-16)/40;return;
    }
}
int main(void){
    enum {W=320,H=200};uint8_t pixels[W*H];memset(pixels,6,sizeof pixels);
    const uint32_t palette[16]={0,0xffffff,0x9f4e44,0x6abfc6,0xa057a3,0x5cab5e,0x50459b,0xc9d487,
        0xa1683c,0x6d5412,0xcb7e75,0x626262,0x898989,0x9ae29b,0x887ecb,0xadadad};
    rc_frame guest={W,H,W,pixels,palette,16,4,3,9},out;
    rc_c64kbd keyboard;rc_c64kbd_init(&keyboard,NULL);rc_osk osk;rc_osk_init(&osk,set_key,&keyboard);rc_osk_open(&osk,1);
    CHECK(rc_osk_draw(&osk,&guest,32,&out));CHECK(osk.top==72&&out.palette==palette&&out.pixels!=pixels);
    CHECK(!memcmp(out.pixels,pixels,W*72)&&!memcmp(out.pixels+W*168,pixels+W*168,W*32));
    unsigned count;const rc_c64_keycap *caps=rc_c64_layout(&count);CHECK(count==66);
    uint8_t found[65]={0};for(unsigned i=0;i<count;++i)found[caps[i].key]=1;
    for(unsigned i=0;i<65;++i)CHECK(found[i]);
    int ax=0,ay=0,bx=0,by=0,sx=0,sy=0;center(&osk,10,&ax,&ay);center(&osk,28,&bx,&by);center(&osk,15,&sx,&sy);
    CHECK(rc_osk_pointer(&osk,1,ax,ay,1)&&rc_osk_pointer(&osk,2,ax,ay,1));
    CHECK(rc_osk_pointer(&osk,1,ax,ay,0));
    uint8_t pa=0xfd,pb=255;rc_c64kbd_scan(&keyboard,&pa,&pb);CHECK(!(pb&4));
    CHECK(rc_osk_pointer(&osk,2,bx,by,1));CHECK(!(keyboard.row[1]&4)&&(keyboard.row[3]&16));
    CHECK(rc_osk_pointer(&osk,2,bx,by,-1));CHECK(!keyboard.row[3]);
    CHECK(rc_osk_pointer(&osk,3,sx,sy,1)&&rc_osk_pointer(&osk,3,sx,sy,0));CHECK(osk.sticky[15]&&(keyboard.row[1]&128));
    CHECK(rc_osk_pointer(&osk,4,ax,ay,1));
    rc_state_writer writer;rc_sw_init(&writer);rc_sw_header(&writer,"c64","c64-pal",NULL,0);rc_sw_begin(&writer,"OSK ",0);
    rc_osk_save(&osk,&writer);rc_sw_end(&writer);CHECK(rc_sw_finish(&writer));
    rc_state_reader reader;char error[80];CHECK(rc_sr_open(&reader,writer.data,writer.size,error,sizeof error)&&rc_sr_chunk(&reader,"OSK ",0));
    rc_osk restored;rc_osk_init(&restored,NULL,NULL);CHECK(rc_osk_load(&restored,&reader));
    rc_osk_release(&osk);rc_osk_bind(&restored,set_key,&keyboard);CHECK((keyboard.row[1]&128)&&!(keyboard.row[1]&4));
    CHECK(restored.visible&&restored.selected==osk.selected&&restored.accepted<0);
    CHECK(rc_osk_key(&restored,"A",1)&&(keyboard.row[1]&4));CHECK(rc_osk_key(&restored,"A",0)&&!(keyboard.row[1]&4));
    CHECK(rc_osk_draw(&restored,&guest,0,&out));center(&restored,64,&ax,&ay);
    CHECK(rc_osk_pointer(&restored,5,ax,ay,1)&&keyboard.restore);CHECK(rc_osk_pointer(&restored,5,ax,ay,-1)&&!keyboard.restore);
    CHECK(rc_osk_key(&restored,"DOWN",1)&&rc_osk_key(&restored,"LEFT",1));
    char text[160];rc_osk_text(&restored,text,sizeof text);CHECK(strstr(text,"ON-SCREEN KEYBOARD OPEN")&&strstr(text,"[LSH]"));
    CHECK(rc_osk_draw(&restored,&guest,0,&out));uint32_t argb[W*H];
    for(unsigned i=0;i<W*H;++i)argb[i]=0xff000000u|out.palette[out.pixels[i]];
    CHECK(rc_png_write(RC_WORK "/osk.png",argb,W,H,W*4,0));
    rc_osk_open(&restored,0);for(unsigned i=0;i<8;++i)CHECK(!keyboard.row[i]);CHECK(!keyboard.restore);
    rc_osk_close(&restored);rc_osk_close(&osk);rc_sw_free(&writer);
    puts("OSK multitouch, matrix contacts, sticky snapshot, controller accept and guest overlay OK");return 0;
}
