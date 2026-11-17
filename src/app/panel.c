#include "panel.h"
#include "../core/font8x8.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void rc_panel_init(rc_panel *p){memset(p,0,sizeof *p);rc_font8x8_build(p->font);}
void rc_panel_close(rc_panel *p){free(p->pixels);p->pixels=NULL;p->capacity=0;}

static void describe(rc_panel *p,const rc_panel_status *s){
    char drive[24];
    if(s->drive_present)snprintf(drive,sizeof drive,"D%u:%02u.%u%s%s",s->drive_device,(s->drive_halftrack/2u)%100u,(s->drive_halftrack&1u)?5u:0u,s->drive_led?"*":"-",s->drive_motor?"M":"-");
    else snprintf(drive,sizeof drive,"D:OFF");
    snprintf(p->lines[0],sizeof p->lines[0],"%s T:%06llu%c%c J:%c",drive,
             (unsigned long long)(s->tape_counter%1000000u),s->tape_record?'R':s->tape_play?'P':'S',s->tape_motor?'M':'-',
             s->joystick_port?(char)('0'+s->joystick_port):'-');
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
    const unsigned rows=source.height>=8u?1u:0u;
    const unsigned left=(source.width-cols*8u)/2u,top=source.height-rows*8u;
    for(unsigned r=0;r<rows;++r){
        const size_t length=strlen(p->lines[r]);
        for(unsigned c=0;c<cols;++c){
            unsigned ch=c<length?(uint8_t)p->lines[r][c]:' ';
            uint8_t ink=14,paper=6;

            for(unsigned y=0;y<8;++y){const unsigned bits=p->font[ch*8u+y];
                for(unsigned x=0;x<8;++x)p->pixels[(size_t)(top+r*8u+y)*source.width+left+c*8u+x]=
                    (uint8_t)(bits&(128u>>x)?ink:paper);
            }
        }
    }
    out->pixels=p->pixels;out->pitch=source.width;out->serial=++p->serial;return 1;
}

size_t rc_panel_text(const rc_panel *p,char *out,size_t capacity){
    size_t n=0;for(unsigned r=0;r<1;++r){
        for(const char *c=p->lines[r];*c&&n+1<capacity;++c)out[n++]=*c;
        if(n+1<capacity)out[n++]='\n';
    }
    if(capacity)out[n]=0;return n;
}
