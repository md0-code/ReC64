#include "georam.h"
#include <stdlib.h>
#include <string.h>
static int capacity(uint32_t n){return !n||n==0x80000u||n==0x100000u||n==0x200000u||n==0x400000u;}
static uint32_t address(const rc_georam *g,uint16_t a){return ((uint32_t)g->bank<<14)|((uint32_t)g->page<<8)|(a&255u);}
int rc_georam_init(rc_georam *g,uint32_t bytes){
    memset(g,0,sizeof *g);if(!capacity(bytes))return 0;
    if(bytes){g->ram=(uint8_t*)calloc(bytes,1);if(!g->ram)return 0;}g->size=bytes;return 1;
}
void rc_georam_free(rc_georam *g){free(g->ram);memset(g,0,sizeof *g);}
void rc_georam_reset(rc_georam *g){(void)g;}
uint8_t rc_georam_drive_mask(const rc_georam *g,uint16_t a){return g->size&&(a&0xff00u)==0xde00u?0xff:0;}
uint8_t rc_georam_read(const rc_georam *g,uint16_t a,uint8_t bus,int side_effects){
    (void)side_effects;return rc_georam_drive_mask(g,a)?g->ram[address(g,a)]:bus;
}
void rc_georam_write(rc_georam *g,uint16_t a,uint8_t v){
    if(!g->size)return;
    if((a&0xff00u)==0xde00u)g->ram[address(g,a)]=v;
    else if((a&0xff80u)==0xdf80u){if(a&1)g->bank=(uint8_t)(v&((g->size>>14)-1));else g->page=v&63;}
}
void rc_georam_save(const rc_georam *g,rc_state_writer *w){
    rc_sw_u32(w,1);rc_sw_u32(w,g->size);rc_sw_u8(w,g->page);rc_sw_u8(w,g->bank);rc_sw_packed(w,g->ram,g->size);
}
int rc_georam_load(rc_georam *g,rc_state_reader *r){
    rc_georam n;uint32_t size;if(rc_sr_u32(r)!=1)return 0;size=rc_sr_u32(r);
    if(!capacity(size)||!rc_georam_init(&n,size))return 0;
    n.page=rc_sr_u8(r);n.bank=rc_sr_u8(r);rc_sr_packed(r,n.ram,n.size);
    if(!rc_sr_ok(r)||n.page>63||(n.size?n.bank>=(n.size>>14):n.page||n.bank)){rc_georam_free(&n);return 0;}
    rc_georam_free(g);*g=n;return 1;
}
