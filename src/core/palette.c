#include "palette.h"
#include <math.h>
#include <string.h>
static const struct {const char *name;uint32_t colors[16];} BUILTINS[]={
    {"color-corrected",{0x000000,0xffffff,0x7d202c,0x4fb3a5,0x84258c,0x339840,0x2a1b9d,0xbfd04a,0x7f410d,0x4c2e00,0xb44f5c,0x3c3c3c,0x646464,0x7ce587,0x6351db,0x939393}},
    {"default",{0x000000,0xffffff,0x68372b,0x70a4b2,0x6f3d86,0x588d43,0x352879,0xb8c76f,0x6f4f25,0x433900,0x9a6759,0x444444,0x6c6c6c,0x9ad284,0x6c5eb5,0x959595}},
    {"default-ntsc",{0x000000,0xffffff,0x67372b,0x70a3b1,0x6f3d86,0x588c42,0x342879,0xb7c66e,0x6f4e25,0x423800,0x996659,0x434343,0x6b6b6b,0x9ad183,0x6b5eb5,0x959595}}
};
/* A colour computed from the VIC-II's signal: one of nine luma steps (in
 * thirty-seconds of white) and, unless it is a grey, a chroma angle in
 * sixteenths of the colour circle, decoded from YUV. `blue` turns the angle
 * of the two blues, which the chip places towards violet. */
static const uint8_t STEP[16]={0,8,2,6,3,5,1,7,3,1,5,2,4,7,4,6},LUMA[9]={0,8,10,12,15,16,20,24,32};
static const int8_t ANGLE[16]={-1,-1,5,13,2,10,0,8,6,7,5,-1,-1,10,0,-1};
static void compute(uint32_t colors[16],double saturation,double gamma,double blue){
    for(unsigned i=0;i<16;++i){
        const double y=LUMA[STEP[i]]/32.0;double u=0,v=0;
        if(ANGLE[i]>=0){const double a=(ANGLE[i]*22.5+(ANGLE[i]?0.0:blue))*3.14159265358979323846/180.0;u=saturation*cos(a);v=saturation*sin(a);}
        const double rgb[3]={y+1.140*v,y-0.395*u-0.581*v,y+2.032*u};colors[i]=0;
        for(unsigned c=0;c<3;++c){const double level=rgb[c]<0?0:rgb[c]>1?1:rgb[c];colors[i]=colors[i]<<8|(uint32_t)(255.0*pow(level,gamma)+0.5);}
    }
}
/* Deeper, more saturated colours and a blue without the violet cast. */
static const uint32_t *tuned(void){static uint32_t colors[16];static int ready;if(!ready){compute(colors,0.215,1.25,-14.0);ready=1;}return colors;}
const uint32_t *rc_palette_builtin(const char *name){if(!strcmp(name,"tuned"))return tuned();for(unsigned i=0;i<3;++i)if(!strcmp(name,BUILTINS[i].name))return BUILTINS[i].colors;return NULL;}
const uint32_t *rc_palette_profile(const char *name,int ntsc){return rc_palette_builtin(!strcmp(name,"default")&&ntsc?"default-ntsc":name);}
void rc_palette_save(const uint32_t colors[16],rc_state_writer *w){rc_sw_u32(w,1);for(unsigned i=0;i<16;++i)rc_sw_u32(w,colors[i]);}
int rc_palette_load(uint32_t colors[16],rc_state_reader *r){if(r->end-r->at<68||rc_sr_u32(r)!=1)return 0;uint32_t parsed[16];for(unsigned i=0;i<16;++i){parsed[i]=rc_sr_u32(r);if(parsed[i]>0xffffffu)return 0;}if(!rc_sr_ok(r))return 0;memcpy(colors,parsed,sizeof parsed);return 1;}
