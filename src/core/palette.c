#include "palette.h"
#include <string.h>
static const struct {const char *name;uint32_t colors[16];} BUILTINS[]={
    {"color-corrected",{0x000000,0xffffff,0x7d202c,0x4fb3a5,0x84258c,0x339840,0x2a1b9d,0xbfd04a,0x7f410d,0x4c2e00,0xb44f5c,0x3c3c3c,0x646464,0x7ce587,0x6351db,0x939393}},
    {"default",{0x000000,0xffffff,0x68372b,0x70a4b2,0x6f3d86,0x588d43,0x352879,0xb8c76f,0x6f4f25,0x433900,0x9a6759,0x444444,0x6c6c6c,0x9ad284,0x6c5eb5,0x959595}},
    {"default-ntsc",{0x000000,0xffffff,0x67372b,0x70a3b1,0x6f3d86,0x588c42,0x342879,0xb7c66e,0x6f4e25,0x423800,0x996659,0x434343,0x6b6b6b,0x9ad183,0x6b5eb5,0x959595}}
};
const uint32_t *rc_palette_builtin(const char *name){for(unsigned i=0;i<3;++i)if(!strcmp(name,BUILTINS[i].name))return BUILTINS[i].colors;return NULL;}
const uint32_t *rc_palette_profile(const char *name,int ntsc){return rc_palette_builtin(!strcmp(name,"default")&&ntsc?"default-ntsc":name);}
void rc_palette_save(const uint32_t colors[16],rc_state_writer *w){rc_sw_u32(w,1);for(unsigned i=0;i<16;++i)rc_sw_u32(w,colors[i]);}
int rc_palette_load(uint32_t colors[16],rc_state_reader *r){if(r->end-r->at<68||rc_sr_u32(r)!=1)return 0;uint32_t parsed[16];for(unsigned i=0;i<16;++i){parsed[i]=rc_sr_u32(r);if(parsed[i]>0xffffffu)return 0;}if(!rc_sr_ok(r))return 0;memcpy(colors,parsed,sizeof parsed);return 1;}
