#include "osk.h"
#include "../core/font8x8.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int modifier(unsigned key){return key==15||key==52||key==58||key==61;}
static void apply(rc_osk *o,unsigned key){
    const unsigned down=o->held[key]||o->sticky[key];
    if(down!=o->sent[key]){o->sent[key]=(uint8_t)down;if(o->set_key)o->set_key(o->ctx,key,(int)down);}
}
static void contact(rc_osk *o,int cap,int down,int latch){
    unsigned count;const rc_c64_keycap *caps=rc_c64_layout(&count);
    if(cap<0||(unsigned)cap>=count)return;
    const unsigned key=caps[cap].key;
    if(down)++o->held[key];
    else {if(o->held[key])--o->held[key];if(latch&&modifier(key))o->sticky[key]^=1;}
    apply(o,key);
}
void rc_osk_init(rc_osk *o,rc_osk_set_key set_key,void *ctx){
    memset(o,0,sizeof *o);o->set_key=set_key;o->ctx=ctx;o->accepted=-1;rc_font8x8_build(o->font);
}
void rc_osk_bind(rc_osk *o,rc_osk_set_key set_key,void *ctx){
    if(o->set_key)for(unsigned i=0;i<65;++i)if(o->sent[i])o->set_key(o->ctx,i,0);
    memset(o->sent,0,sizeof o->sent);o->set_key=set_key;o->ctx=ctx;
    for(unsigned i=0;i<65;++i)apply(o,i);
}
void rc_osk_release(rc_osk *o){
    memset(o->contacts,0,sizeof o->contacts);memset(o->held,0,sizeof o->held);memset(o->sticky,0,sizeof o->sticky);
    o->accepted=-1;for(unsigned i=0;i<65;++i)apply(o,i);
}
void rc_osk_open(rc_osk *o,int visible){if(!visible)rc_osk_release(o);o->visible=!!visible;}
void rc_osk_close(rc_osk *o){rc_osk_release(o);free(o->pixels);o->pixels=NULL;o->capacity=0;o->visible=0;}

static void bounds(const rc_osk *o,const rc_c64_keycap *c,int *x,int *y,int *w,int *h){
    *x=o->left+c->x*o->width/80;*y=o->top+16+c->y*(o->height-16)/20;
    *w=o->left+(c->x+c->w)*o->width/80-*x;*h=o->top+16+(c->y+c->h)*(o->height-16)/20-*y;
}
static int hit(const rc_osk *o,int x,int y){
    unsigned count;const rc_c64_keycap *caps=rc_c64_layout(&count);
    for(unsigned i=0;i<count;++i){int kx,ky,w,h;bounds(o,&caps[i],&kx,&ky,&w,&h);
        if(x>=kx&&y>=ky&&x<kx+w&&y<ky+h)return (int)i;
    }return -1;
}
int rc_osk_pointer(rc_osk *o,int64_t id,int x,int y,int down){
    if(!o->visible)return 0;
    rc_osk_contact *p=NULL,*free_slot=NULL;
    for(unsigned i=0;i<RC_OSK_POINTERS;++i){if(o->contacts[i].used&&o->contacts[i].id==id)p=&o->contacts[i];
        if(!o->contacts[i].used&&!free_slot)free_slot=&o->contacts[i];}
    const int cap=hit(o,x,y);
    if(down<=0){if(!p)return 0;contact(o,p->cap,0,down==0&&p->cap==cap);p->used=0;return 1;}
    if(!p){if(cap<0||!free_slot)return 0;p=free_slot;p->id=id;p->cap=-1;p->used=1;}
    if(p->cap!=cap){contact(o,p->cap,0,0);p->cap=cap;contact(o,cap,1,0);}
    if(cap>=0)o->selected=cap;return 1;
}
static void navigate(rc_osk *o,int dx,int dy){
    unsigned count;const rc_c64_keycap *caps=rc_c64_layout(&count);
    const rc_c64_keycap *from=&caps[o->selected];
    const int fx=2*from->x+from->w,fy=2*from->y+from->h;
    int best=-1,best_score=0;
    for(unsigned i=0;i<count;++i){const int x=2*caps[i].x+caps[i].w-fx,y=2*caps[i].y+caps[i].h-fy;
        const int forward=dx?x*dx:y*dy,side=dx?abs(y):abs(x);
        if(forward<=0)continue;
        const int score=forward*forward+side*side*4;
        if(best<0||score<best_score){best=(int)i;best_score=score;}
    }
    if(best>=0)o->selected=best;
}
int rc_osk_key(rc_osk *o,const char *name,int down){
    if(!o->visible||!name)return 0;
    if(!strcmp(name,"ESCAPE")||!strcmp(name,"B")){if(down)rc_osk_open(o,0);return 1;}
    if(!strcmp(name,"A")||!strcmp(name,"RETURN")||!strcmp(name,"SPACE")){
        if(down&&o->accepted<0){o->accepted=o->selected;contact(o,o->accepted,1,0);}
        else if(!down&&o->accepted>=0){contact(o,o->accepted,0,1);o->accepted=-1;}return 1;
    }
    int dx=0,dy=0;
    if(!strcmp(name,"LEFT"))dx=-1;else if(!strcmp(name,"RIGHT"))dx=1;
    else if(!strcmp(name,"UP"))dy=-1;else if(!strcmp(name,"DOWN"))dy=1;else return 0;
    if(down)navigate(o,dx,dy);return 1;
}

static const char *long_legend(const rc_c64_keycap *c){
    switch(c->key){case 0:return "INST/DEL";case 1:return "RETURN";case 2:return "CURSOR RIGHT";
    case 7:return "CURSOR DOWN";case 15:return c->y==8?"SHIFT LOCK":"LEFT SHIFT";
    case 48:return "POUND";case 51:return "CLR/HOME";case 52:return "RIGHT SHIFT";
    case 54:return "UP ARROW";case 57:return "LEFT ARROW";case 58:return "CTRL";
    case 61:return "COMMODORE";case 63:return "RUN/STOP";case 64:return "RESTORE";
    default:return c->legend;}
}
static void details(const rc_osk *o,char line[80]){
    const rc_c64_keycap *caps=rc_c64_layout(NULL);
    snprintf(line,80,"%s%s%s%s%s",long_legend(&caps[o->selected]),
             o->sticky[15]?" [LSH]":"",o->sticky[52]?" [RSH]":"",o->sticky[58]?" [CTRL]":"",o->sticky[61]?" [C=]":"");
}
static void glyph(rc_osk *o,unsigned pitch,int x,int y,unsigned ch,unsigned ink,unsigned paper){
    for(unsigned r=0;r<8;++r)for(unsigned c=0;c<8;++c)
        o->pixels[(size_t)(y+(int)r)*pitch+(unsigned)(x+(int)c)]=(uint8_t)(o->font[ch*8+r]&(128u>>c)?ink:paper);
}
static void text(rc_osk *o,unsigned pitch,int x,int y,int width,const char *s,unsigned ink,unsigned paper){
    for(;*s&&width>=8;++s,x+=8,width-=8)glyph(o,pitch,x,y,(uint8_t)*s,ink,paper);
}
int rc_osk_draw(rc_osk *o,const rc_frame *guest,unsigned bottom_margin,rc_frame *out){
    const rc_frame source=*guest;*out=source;
    if(!o->visible)return 1;
    if(!source.pixels||!source.palette||source.colors<16||source.width<80||source.pitch<source.width||
       source.height<96||bottom_margin>source.height-96||source.height>SIZE_MAX/source.width)return 0;
    const size_t size=(size_t)source.width*source.height;
    if(size>o->capacity){uint8_t *next=realloc(o->pixels,size);if(!next)return 0;o->pixels=next;o->capacity=size;}
    for(unsigned y=0;y<source.height;++y)memcpy(o->pixels+(size_t)y*source.width,source.pixels+(size_t)y*source.pitch,source.width);
    o->width=source.width<320?(int)source.width:320;o->height=96;o->left=((int)source.width-o->width)/2;
    o->top=(int)(source.height-bottom_margin)-o->height;
    for(int y=0;y<o->height;++y)memset(o->pixels+(size_t)(o->top+y)*source.width+(unsigned)o->left,y<8?14:6,(size_t)o->width);
    text(o,source.width,o->left,o->top,o->width," ON-SCREEN KEYBOARD",6,14);
    char line[80];details(o,line);text(o,source.width,o->left,o->top+8,o->width,line,14,6);
    const uint8_t stripes[5]={2,8,7,5,14};
    if(o->width>=320)for(unsigned i=0;i<5;++i)glyph(o,source.width,o->left+280+(int)i*8,o->top,RC_GLYPH_STRIPE,stripes[i],0);
    unsigned count;const rc_c64_keycap *caps=rc_c64_layout(&count);
    for(unsigned i=0;i<count;++i){int x,y,w,h;bounds(o,&caps[i],&x,&y,&w,&h);
        const unsigned key=caps[i].key;const unsigned paper=o->sent[key]?14:0,ink=o->sent[key]?6:14;
        for(int row=0;row<h-1;++row)memset(o->pixels+(size_t)(y+row)*source.width+(unsigned)x,(int)paper,(size_t)(w-1));
        const size_t length=strlen(caps[i].legend);int chars=w/8;
        if(chars>(int)length)chars=(int)length;
        const int tx=x+(w-chars*8)/2,ty=y+(h-8)/2;
        if(h>=8)text(o,source.width,tx,ty,chars*8,caps[i].legend,ink,paper);
        if((int)i==o->selected)for(int col=0;col<w-1;++col)o->pixels[(size_t)(y+h-2)*source.width+(unsigned)(x+col)]=7;
    }
    out->pixels=o->pixels;out->pitch=source.width;out->serial=++o->serial;return 1;
}
size_t rc_osk_text(const rc_osk *o,char *out,size_t capacity){
    char detail[80];details(o,detail);int n=snprintf(out,capacity,"ON-SCREEN KEYBOARD %s\n%s\n",o->visible?"OPEN":"CLOSED",detail);
    return n<0?0:(size_t)n<capacity?(size_t)n:capacity?capacity-1:0;
}
void rc_osk_save(const rc_osk *o,rc_state_writer *w){
    rc_sw_u8(w,(uint8_t)o->visible);rc_sw_u8(w,(uint8_t)o->selected);
    rc_sw_u8(w,(uint8_t)(o->sticky[15]|(o->sticky[52]<<1)|(o->sticky[58]<<2)|(o->sticky[61]<<3)));
}
int rc_osk_load(rc_osk *o,rc_state_reader *r){
    unsigned count;rc_c64_layout(&count);
    const unsigned visible=rc_sr_u8(r),selected=rc_sr_u8(r),sticky=rc_sr_u8(r);
    if(!rc_sr_ok(r)||visible>1||selected>=count||sticky>15||(!visible&&sticky))return 0;
    rc_osk_release(o);o->visible=(int)visible;o->selected=(int)selected;
    o->sticky[15]=(uint8_t)(sticky&1u);o->sticky[52]=(uint8_t)((sticky>>1)&1u);
    o->sticky[58]=(uint8_t)((sticky>>2)&1u);o->sticky[61]=(uint8_t)((sticky>>3)&1u);
    for(unsigned i=0;i<65;++i)apply(o,i);return 1;
}
