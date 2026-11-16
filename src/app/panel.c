#include "panel.h"
#include "../core/font8x8.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void rc_panel_init(rc_panel *p){memset(p,0,sizeof *p);rc_font8x8_build(p->font);}
void rc_panel_close(rc_panel *p){free(p->pixels);p->pixels=NULL;p->capacity=0;}

static void describe(rc_panel *p,const rc_panel_status *s){
    snprintf(p->lines[0],sizeof p->lines[0]," REC64 STATUS");
    if(s->drive_present)
        snprintf(p->lines[1],sizeof p->lines[1],"1541 #%02u TRK %02u.%u LED %-3s MOTOR %s",
                 s->drive_device%100u,(s->drive_halftrack/2u)%100u,(s->drive_halftrack&1u)?5u:0u,
                 s->drive_led?"ON":"OFF",s->drive_motor?"ON":"OFF");
    else snprintf(p->lines[1],sizeof p->lines[1],"1541 OFF");
    const char *transport=s->tape_record?"RECORD":s->tape_play?"PLAY":"STOP";
    if(s->tape_counter<UINT64_C(1000000000000))
        snprintf(p->lines[2],sizeof p->lines[2],"TAPE POS %010llu MOTOR %-3s %s",
             (unsigned long long)s->tape_counter,s->tape_motor?"ON":"OFF",transport);
    else snprintf(p->lines[2],sizeof p->lines[2],"TAPE POS %llu M:%u %s",
             (unsigned long long)s->tape_counter,s->tape_motor?1u:0u,
             s->tape_record?"RECORD":s->tape_play?"PLAY":"STOP");
    if(s->joystick_port==1||s->joystick_port==2)
        snprintf(p->lines[3],sizeof p->lines[3],"KEYBOARD JOYSTICK PORT %u",s->joystick_port);
    else snprintf(p->lines[3],sizeof p->lines[3],"KEYBOARD JOYSTICK OFF");
}

int rc_panel_draw(rc_panel *p,const rc_frame *guest,const rc_panel_status *s,rc_frame *out){
    const rc_frame source=*guest;*out=source;
    if(!s||!source.pixels||!source.palette||source.colors<16||!source.width||!source.height||
       source.pitch<source.width||source.height>SIZE_MAX/source.width)return 0;
    const size_t size=(size_t)source.width*source.height;
    if(size>p->capacity){uint8_t *next=realloc(p->pixels,size);if(!next)return 0;p->pixels=next;p->capacity=size;}
    for(unsigned y=0;y<source.height;++y)
        memcpy(p->pixels+(size_t)y*source.width,source.pixels+(size_t)y*source.pitch,source.width);
    describe(p,s);
    const unsigned cols=source.width/8u<40u?source.width/8u:40u;
    const unsigned rows=source.height/8u<4u?source.height/8u:4u;
    const unsigned left=(source.width-cols*8u)/2u,top=source.height-rows*8u;
    for(unsigned r=0;r<rows;++r){
        const size_t length=strlen(p->lines[r]);
        for(unsigned c=0;c<cols;++c){
            unsigned ch=c<length?(uint8_t)p->lines[r][c]:' ';
            uint8_t ink=r?14:6,paper=r?6:14;
            if(!r&&c>=35){const uint8_t stripes[5]={2,8,7,5,14};ch=RC_GLYPH_STRIPE;ink=stripes[c-35];paper=0;}
            for(unsigned y=0;y<8;++y){const unsigned bits=p->font[ch*8u+y];
                for(unsigned x=0;x<8;++x)p->pixels[(size_t)(top+r*8u+y)*source.width+left+c*8u+x]=
                    (uint8_t)(bits&(128u>>x)?ink:paper);
            }
        }
    }
    out->pixels=p->pixels;out->pitch=source.width;out->serial=++p->serial;return 1;
}

size_t rc_panel_text(const rc_panel *p,char *out,size_t capacity){
    size_t n=0;for(unsigned r=0;r<4;++r){
        for(const char *c=p->lines[r];*c&&n+1<capacity;++c)out[n++]=*c;
        if(n+1<capacity)out[n++]='\n';
    }
    if(capacity)out[n]=0;return n;
}
