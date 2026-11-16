#include "palette.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
static const struct {const char *name;uint32_t colors[16];} BUILTINS[]={
    {"cjam",{0x000000,0xffffff,0x7d202c,0x4fb3a5,0x84258c,0x339840,0x2a1b9d,0xbfd04a,0x7f410d,0x4c2e00,0xb44f5c,0x3c3c3c,0x646464,0x7ce587,0x6351db,0x939393}},
    {"colodore",{0x000000,0xffffff,0x96282e,0x5bd6ce,0x9f2dad,0x41b936,0x2724c4,0xeff347,0x9f4815,0x5e3500,0xda5f66,0x474747,0x787878,0x91ff84,0x6864ff,0xaeaeae}},
    {"pepto-pal",{0x000000,0xffffff,0x68372b,0x70a4b2,0x6f3d86,0x588d43,0x352879,0xb8c76f,0x6f4f25,0x433900,0x9a6759,0x444444,0x6c6c6c,0x9ad284,0x6c5eb5,0x959595}},
    {"pepto-ntsc",{0x000000,0xffffff,0x67372b,0x70a3b1,0x6f3d86,0x588c42,0x342879,0xb7c66e,0x6f4e25,0x423800,0x996659,0x434343,0x6b6b6b,0x9ad183,0x6b5eb5,0x959595}}
};
const uint32_t *rc_palette_builtin(const char *name){for(unsigned i=0;i<4;++i)if(!strcmp(name,BUILTINS[i].name))return BUILTINS[i].colors;return NULL;}
int rc_palette_parse(const uint8_t *bytes,size_t size,uint32_t colors[16],char *error,size_t cap){
    uint32_t parsed[16];unsigned count=0;size_t at=0;
    if(memchr(bytes,0,size))goto invalid;
    while(at<size){char line[256];size_t n=0;
        while(at<size&&bytes[at]!='\n'){if(n+1>=sizeof line){if(cap)snprintf(error,cap,"VPL line is too long");return 0;}line[n++]=(char)bytes[at++];}
        if(at<size)++at;line[n]=0;char *comment=strchr(line,'#');if(comment)*comment=0;
        char *p=line;while(isspace((unsigned char)*p))++p;if(!*p)continue;
        if(count==16){if(cap)snprintf(error,cap,"VPL must contain exactly sixteen colours");return 0;}
        unsigned c[4]={0};unsigned fields=0;
        while(*p){if(fields==4||!isxdigit((unsigned char)*p))goto invalid;char *end;const unsigned long v=strtoul(p,&end,16);if(v>255||(*end&&!isspace((unsigned char)*end)))goto invalid;c[fields++]=(unsigned)v;p=end;while(isspace((unsigned char)*p))++p;}
        if(fields<3)goto invalid;parsed[count++]=c[0]<<16|c[1]<<8|c[2];
    }
    if(count!=16)goto invalid;memcpy(colors,parsed,sizeof parsed);return 1;
invalid:if(cap)snprintf(error,cap,"Invalid VPL: expected sixteen RGB hex triples");return 0;
}
size_t rc_palette_write(const uint32_t colors[16],char *out,size_t cap){
    const size_t needed=16u*9u;if(cap<=needed)return 0;
    for(unsigned i=0;i<16;++i)snprintf(out+i*9u,cap-i*9u,"%02X %02X %02X\n",colors[i]>>16&255u,colors[i]>>8&255u,colors[i]&255u);
    return needed;
}
void rc_palette_save(const uint32_t colors[16],rc_state_writer *w){rc_sw_u32(w,1);for(unsigned i=0;i<16;++i)rc_sw_u32(w,colors[i]);}
int rc_palette_load(uint32_t colors[16],rc_state_reader *r){if(r->end-r->at<68||rc_sr_u32(r)!=1)return 0;uint32_t parsed[16];for(unsigned i=0;i<16;++i){parsed[i]=rc_sr_u32(r);if(parsed[i]>0xffffffu)return 0;}if(!rc_sr_ok(r))return 0;memcpy(colors,parsed,sizeof parsed);return 1;}
