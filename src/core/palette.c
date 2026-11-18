#include "palette.h"
#include <math.h>
#include <string.h>
/* The sixteen colours are computed from the VIC-II's signal. Each is one of
 * nine luma steps (in thirty-seconds of white) and, unless it is a grey, a
 * chroma angle in sixteenths of the colour circle, decoded from YUV; the
 * steps and angles are the published measurements of the chip. `blue` turns
 * the angle of the two blues, which the chip places towards violet. */
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
/* default: the chip's colours. color-corrected: more saturation, deeper
 * mid-tones and a blue without the violet cast. */
const uint32_t *rc_palette_builtin(const char *name){
    static uint32_t colors[2][16];static int ready;
    if(!ready){compute(colors[0],0.135,1.15,0.0);compute(colors[1],0.24,1.25,-14.0);ready=1;}
    return !strcmp(name,"default")?colors[0]:!strcmp(name,"color-corrected")?colors[1]:NULL;
}
void rc_palette_save(const uint32_t colors[16],rc_state_writer *w){rc_sw_u32(w,1);for(unsigned i=0;i<16;++i)rc_sw_u32(w,colors[i]);}
int rc_palette_load(uint32_t colors[16],rc_state_reader *r){if(r->end-r->at<68||rc_sr_u32(r)!=1)return 0;uint32_t parsed[16];for(unsigned i=0;i<16;++i){parsed[i]=rc_sr_u32(r);if(parsed[i]>0xffffffu)return 0;}if(!rc_sr_ok(r))return 0;memcpy(colors,parsed,sizeof parsed);return 1;}
