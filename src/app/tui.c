#include "tui.h"
#include "../core/font8x8.h"
#include <ctype.h>
#include <string.h>
#include "rc_version.h"
void rc_tui_init(rc_tui *t){memset(t,0,sizeof *t);rc_font8x8_build(t->font);rc_tui_clear(t);}
void rc_tui_clear(rc_tui *t){memset(t->ch,' ',sizeof t->ch);memset(t->ink,14,sizeof t->ink);memset(t->paper,6,sizeof t->paper);}
void rc_tui_put(rc_tui *t,unsigned row,unsigned col,const char *text,uint8_t ink,uint8_t paper){
    if(row>=25)return;for(;*text&&col<40;++text,++col){t->ch[row][col]=(uint8_t)toupper((unsigned char)*text);t->ink[row][col]=ink;t->paper[row][col]=paper;}
}
static void cell(rc_tui *t,unsigned y,unsigned x,unsigned code){if(y<25&&x<40)t->ch[y][x]=(uint8_t)code;}
void rc_tui_box(rc_tui *t,unsigned y,unsigned x,unsigned h,unsigned w){
    if(h<2||w<2)return;
    for(unsigned i=1;i+1<w;++i){cell(t,y,x+i,RC_GLYPH_LINE_H);cell(t,y+h-1,x+i,RC_GLYPH_LINE_H);}
    for(unsigned i=1;i+1<h;++i){cell(t,y+i,x,RC_GLYPH_LINE_V);cell(t,y+i,x+w-1,RC_GLYPH_LINE_V);}
    cell(t,y,x,RC_GLYPH_CORNER_TL);cell(t,y,x+w-1,RC_GLYPH_CORNER_TR);cell(t,y+h-1,x,RC_GLYPH_CORNER_BL);cell(t,y+h-1,x+w-1,RC_GLYPH_CORNER_BR);
}
void rc_tui_bar(rc_tui *t,unsigned row,const char *title){
    rc_tui_put(t,row,0,"                                        ",6,14);
    rc_tui_put(t,row,1,"REC64 " RC_VERSION,6,14);
    const size_t length=strlen(title);rc_tui_put(t,row,length<24?39u-(unsigned)length:15u,title,6,14);
}
void rc_tui_frame(rc_tui *t,const uint32_t palette[16],rc_frame *f){
    memset(t->pixels,14,sizeof t->pixels);
    for(unsigned r=0;r<25;++r)for(unsigned c=0;c<40;++c)for(unsigned y=0;y<8;++y){const uint8_t bits=t->font[t->ch[r][c]*8u+y];
        for(unsigned x=0;x<8;++x)t->pixels[(36u+r*8u+y)*384+32+c*8+x]=(uint8_t)(bits&(128u>>x)?t->ink[r][c]:t->paper[r][c]);}
    *f=(rc_frame){384,272,384,t->pixels,palette,16,4,3,++t->serial};
}
size_t rc_tui_text(const rc_tui *t,char *out,size_t cap){size_t n=0;for(unsigned r=0;r<25;++r){unsigned last=40;while(last&&t->ch[r][last-1]==' ')--last;
    for(unsigned c=0;c<last&&n+1<cap;++c){const unsigned ch=t->ch[r][c];out[n++]=ch<128?(char)ch:' ';}if(n+1<cap)out[n++]='\n';}if(cap)out[n]=0;return n;}
int rc_tui_overlay(rc_tui *t,unsigned rows,const rc_frame *guest,rc_frame *out){
    const rc_frame source=*guest;*out=source;
    if(!source.pixels||source.width<320||rows>25||source.height<rows*8||source.pitch<source.width||source.width*source.height>sizeof t->pixels)return 0;
    for(unsigned y=0;y<source.height;++y)memcpy(t->pixels+(size_t)y*source.width,source.pixels+(size_t)y*source.pitch,source.width);
    const unsigned left=(source.width-320)/2,top=(source.height-rows*8)/2;
    for(unsigned r=0;r<rows;++r)for(unsigned c=0;c<40;++c)for(unsigned y=0;y<8;++y){const uint8_t bits=t->font[t->ch[r][c]*8u+y];
        for(unsigned x=0;x<8;++x)t->pixels[(size_t)(top+r*8+y)*source.width+left+c*8+x]=(uint8_t)(bits&(128u>>x)?t->ink[r][c]:t->paper[r][c]);}
    out->pixels=t->pixels;out->pitch=source.width;out->serial=++t->serial;return 1;
}
void rc_tui_status(const uint8_t font[2048],uint32_t *argb,unsigned w,unsigned h,const uint32_t palette[16],const char *text){
    if(w<24||h<8)return;const unsigned cols=w/8-2;unsigned c=0;
    for(unsigned y=h-8;y<h;++y)for(unsigned x=0;x<w;++x)argb[y*w+x]=0xff000000u|palette[6];
    for(;*text&&c<cols;++text,++c){const unsigned ch=(unsigned char)toupper((unsigned char)*text);for(unsigned y=0;y<8;++y)for(unsigned x=0;x<8;++x)argb[(h-8+y)*w+8+c*8+x]=0xff000000u|palette[font[ch*8+y]&(128u>>x)?14:6];}
}
