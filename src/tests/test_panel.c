/* One headless overlay flow: live fields, shared font and guest preservation. */
#include "../app/panel.h"
#include "../core/font8x8.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define CHECK(x) do{if(!(x)){fprintf(stderr,"line %d: %s\n",__LINE__,#x);return 1;}}while(0)
int main(void){
    enum {W=320,H=200,PITCH=328};
    uint8_t guest[PITCH*H];uint32_t palette[16]={0};
    for(size_t i=0;i<sizeof guest;++i)guest[i]=(uint8_t)(i%16);
    rc_frame frame={W,H,PITCH,guest,palette,16,4,3,17},out;
    rc_panel panel;rc_panel_init(&panel);
    rc_panel_status status={1,8,37,1,0,1234,1,1,0,2};
    CHECK(rc_panel_draw(&panel,&frame,&status,&out));
    CHECK(out.width==W&&out.height==H&&out.pitch==W&&out.palette==palette&&out.colors==16);
    CHECK(out.aspect_w==4&&out.aspect_h==3&&out.pixels!=guest);
    for(unsigned y=0;y<H-32;++y)CHECK(!memcmp(out.pixels+y*W,guest+y*PITCH,W));
    for(size_t i=0;i<sizeof guest;++i)CHECK(guest[i]==(uint8_t)(i%16));
    char text[256];rc_panel_text(&panel,text,sizeof text);
    CHECK(strstr(text,"1541 #08 TRK 18.5 LED ON  MOTOR OFF"));
    CHECK(strstr(text,"TAPE POS 0000001234 MOTOR ON  PLAY"));
    CHECK(strstr(text,"KEYBOARD JOYSTICK PORT 2"));
    /* The title R uses exactly the font shared with the setup TUI. */
    for(unsigned y=0;y<8;++y)for(unsigned x=0;x<8;++x)
        CHECK(out.pixels[(H-32+y)*W+8+x]==(rc_font8x8_ascii['R'-32][y]&(128u>>x)?6:14));
    uint64_t serial=out.serial;
    status.drive_halftrack=38;status.drive_led=0;status.drive_motor=1;
    status.tape_counter=2345;status.tape_play=0;status.tape_record=1;status.joystick_port=1;
    guest[0]=7;CHECK(rc_panel_draw(&panel,&frame,&status,&out));
    CHECK(out.serial!=serial&&out.pixels[0]==7);
    rc_panel_text(&panel,text,sizeof text);
    CHECK(strstr(text,"TRK 19.0 LED OFF MOTOR ON"));
    CHECK(strstr(text,"TAPE POS 0000002345 MOTOR ON  RECORD")&&strstr(text,"PORT 1"));
    rc_panel_close(&panel);
    puts("Status panel live fields, shared glyphs and cropped guest preservation OK");return 0;
}
