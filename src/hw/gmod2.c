#include "gmod2.h"
#include "../core/media/image.h"
#include <stdlib.h>
#include <string.h>
static const rc_cart_ops *mapper_ops(void);
void rc_gmod2_init(rc_gmod2 *g){memset(g,0,sizeof *g);g->ba=1;}
void rc_gmod2_close(rc_gmod2 *g){if(g->port&&g->port->ctx==g&&g->port->ops==mapper_ops()){g->port->ops=NULL;g->port->ctx=NULL;}free(g->storage);rc_gmod2_init(g);}
static void reset(void *ctx){rc_gmod2 *g=ctx;g->control=g->write_cycle=0;g->ba=1;rc_m93c86_pins(&g->eeprom,0,0,0);}
int rc_gmod2_attach(rc_gmod2 *g,rc_cart *port,uint32_t hz,char *error,size_t cap){
    if(!port||!port->crt.image||port->crt.hardware!=RC_CRT_GMOD2||!hz)return rc_image_error(error,cap,"Not a GMod2 CRT or invalid clock");
    for(size_t i=0;i<port->crt.count;++i){const rc_crt_chip *ch=&port->crt.chips[i];if(ch->bank>=64||ch->size!=0x2000||ch->address!=0x8000||ch->type==1||ch->offset==SIZE_MAX)return rc_image_error(error,cap,"Invalid GMod2 CHIP geometry");}
    uint8_t *storage=malloc(RC_AM29F040_SIZE);if(!storage)return rc_image_error(error,cap,"Out of memory");memset(storage,255,RC_AM29F040_SIZE);
    for(size_t i=0;i<port->crt.count;++i){const rc_crt_chip *ch=&port->crt.chips[i];memcpy(storage+ch->bank*0x2000u,port->crt.image->data+ch->offset,0x2000);}
    rc_gmod2_close(g);g->port=port;g->storage=storage;rc_am29f040_init(&g->flash,storage,hz);rc_m93c86_init(&g->eeprom,hz);rc_cart_bind(port,mapper_ops(),g);return 1;
}
int rc_gmod2_sync(rc_gmod2 *g){
    if(!g->storage||!g->port)return 0;const uint64_t dirty=g->flash.dirty_banks;if(!dirty)return 1;
    rc_crt *crt=&g->port->crt;rc_media *m=crt->image;if(!m||!m->writable)return 0;
    size_t missing=0;for(unsigned b=0;b<64;++b)if(((dirty>>b)&1u)&&!rc_crt_find(crt,b,0x8000))++missing;
    rc_crt_chip *chips=crt->chips;uint8_t *bytes=m->data;size_t size=m->size,count=crt->count;
    if(missing){if(size>RC_MEDIA_LIMIT-missing*0x2010u)return 0;chips=malloc((count+missing)*sizeof *chips);bytes=malloc(size+missing*0x2010u);if(!chips||!bytes){free(chips);free(bytes);return 0;}memcpy(chips,crt->chips,count*sizeof *chips);memcpy(bytes,m->data,size);}
    for(unsigned b=0;b<64;++b)if((dirty>>b)&1u){const rc_crt_chip *ch=rc_crt_find(crt,b,0x8000);size_t offset;
        if(ch)offset=ch->offset;
        else {uint8_t *p=bytes+size;memset(p,0,16);memcpy(p,"CHIP",4);p[6]=0x20;p[7]=0x10;p[9]=2;p[11]=(uint8_t)b;p[12]=0x80;p[14]=0x20;offset=size+16;chips[count++]=(rc_crt_chip){2,(uint16_t)b,0x8000,0x2000,offset};size+=0x2010;}
        memcpy(bytes+offset,g->storage+b*0x2000u,0x2000);
    }
    if(missing){free(crt->chips);free(m->data);crt->chips=chips;crt->count=count;m->data=bytes;m->size=size;}
    m->dirty=1;g->flash.dirty_banks=0;return 1;
}
static void observe(void *ctx,uint16_t a,int write,int effects,int vic){rc_gmod2 *g=ctx;if(effects)g->write_cycle=(uint8_t)(write&&!vic&&a>=0x8000&&(g->control&0xc0u)==0xc0u);}
static rc_cart_lines lines(const void *ctx){const rc_gmod2 *g=ctx;rc_cart_lines p={1,(g->control>>6)&1u,1,1,1,g->ba};if(g->write_cycle){p.game=0;p.exrom=1;}return p;}
static uint8_t mask(const void *ctx,uint16_t a,unsigned sel,int vic){const rc_gmod2 *g=ctx;(void)vic;if(sel&RC_CART_ROML)return 255;return (sel&RC_CART_IO1)&&a==0xde00&&g->eeprom.q<2?128:0;}
static uint8_t read(void *ctx,uint16_t a,unsigned sel,uint8_t bus,int effects,int vic){rc_gmod2 *g=ctx;(void)vic;if(sel&RC_CART_ROML)return rc_am29f040_read(&g->flash,(g->control&63u)*0x2000u+(a&0x1fffu),effects);if((sel&RC_CART_IO1)&&a==0xde00&&g->eeprom.q<2)return (uint8_t)((bus&127u)|(g->eeprom.q<<7));return bus;}
static void write(void *ctx,uint16_t a,unsigned sel,uint8_t v){rc_gmod2 *g=ctx;
    if((sel&RC_CART_IO1)&&a==0xde00){g->control=v;rc_m93c86_pins(&g->eeprom,v&64u,v&32u,v&16u);}
    else if((sel&RC_CART_ROMH)&&(g->control&0xc0u)==0xc0u&&g->port->crt.image->writable)rc_am29f040_write(&g->flash,(g->control&63u)*0x2000u+(a&0x1fffu),v);
}
static void tick(void *ctx,uint64_t cycles,int ba){rc_gmod2 *g=ctx;g->ba=(uint8_t)(ba!=0);rc_am29f040_tick(&g->flash,cycles);rc_m93c86_tick(&g->eeprom,cycles);if(g->flash.dirty_banks)rc_gmod2_sync(g);}
static void save(const void *ctx,rc_state_writer *w){const rc_gmod2 *g=ctx;rc_sw_u32(w,1);rc_sw_u8(w,g->control);rc_sw_u8(w,g->ba);rc_sw_u8(w,g->write_cycle);rc_am29f040_save(&g->flash,w);rc_m93c86_save(&g->eeprom,w);}
static int load(void *ctx,rc_state_reader *r){rc_gmod2 *g=ctx;if(rc_sr_u32(r)!=1)return 0;rc_gmod2 n=*g;n.control=rc_sr_u8(r);n.ba=rc_sr_u8(r);n.write_cycle=rc_sr_u8(r);if(n.ba>1||n.write_cycle>1)return 0;
    uint8_t *memory=malloc(RC_AM29F040_SIZE);if(!memory)return 0;n.flash.memory=memory;
    if(!rc_am29f040_load(&n.flash,r)||!rc_m93c86_load(&n.eeprom,r)){free(memory);return 0;}memcpy(g->storage,memory,RC_AM29F040_SIZE);free(memory);n.flash.memory=g->storage;*g=n;return 1;
}
static const rc_cart_ops *mapper_ops(void){static const rc_cart_ops ops={.observe=observe,.drive_mask=mask,.reset=reset,.read=read,.write=write,.lines=lines,.tick=tick,.save=save,.load=load};return &ops;}
