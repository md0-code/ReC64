#include "easyflash.h"
#include "../core/media/image.h"
#include <stdlib.h>
#include <string.h>
static const rc_cart_ops *mapper_ops(void);
void rc_easyflash_init(rc_easyflash *e){memset(e,0,sizeof *e);e->boot=e->ba=1;}
void rc_easyflash_close(rc_easyflash *e){
    if(e->port&&e->port->ctx==e&&e->port->ops==mapper_ops()){e->port->ops=NULL;e->port->ctx=NULL;}
    free(e->storage);rc_easyflash_init(e);
}
static void reset(void *ctx){rc_easyflash *e=ctx;e->bank=e->control=0;e->ba=1;/* Flash has no C64 RESET input. */}
int rc_easyflash_attach(rc_easyflash *e,rc_cart *port,uint32_t hz,char *error,size_t cap){
    if(!port||!port->crt.image||port->crt.hardware!=RC_CRT_EASYFLASH||!hz)return rc_image_error(error,cap,"Not an EasyFlash CRT or invalid clock");
    for(size_t i=0;i<port->crt.count;++i){const rc_crt_chip *ch=&port->crt.chips[i];
        if(ch->bank>=64||ch->size!=0x2000||(ch->address!=0x8000&&ch->address!=0xa000&&ch->address!=0xe000)||ch->type==1||ch->offset==SIZE_MAX)return rc_image_error(error,cap,"Invalid EasyFlash CHIP geometry");
        for(size_t j=0;j<i;++j){const rc_crt_chip *prior=&port->crt.chips[j];if(prior->bank==ch->bank&&(prior->address!=0x8000)==(ch->address!=0x8000))return rc_image_error(error,cap,"Duplicate EasyFlash bank");}
    }
    uint8_t *storage=malloc(2u*RC_AM29F040_SIZE);if(!storage)return rc_image_error(error,cap,"Out of memory");memset(storage,255,2u*RC_AM29F040_SIZE);
    for(size_t i=0;i<port->crt.count;++i){const rc_crt_chip *ch=&port->crt.chips[i];unsigned which=ch->address!=0x8000;memcpy(storage+which*RC_AM29F040_SIZE+ch->bank*0x2000u,port->crt.image->data+ch->offset,0x2000);}
    rc_easyflash_close(e);e->port=port;e->storage=storage;
    for(unsigned i=0;i<2;++i)rc_am29f040_init(&e->flash[i],storage+i*RC_AM29F040_SIZE,hz);
    rc_cart_bind(port,mapper_ops(),e);return 1;
}
static const rc_crt_chip *bank_chip(const rc_crt *crt,unsigned which,unsigned bank){
    for(size_t i=0;i<crt->count;++i){const rc_crt_chip *ch=&crt->chips[i];if(ch->bank==bank&&(ch->address!=0x8000)==(which!=0))return ch;}return NULL;
}
static void be16(uint8_t *p,unsigned n){p[0]=(uint8_t)(n>>8);p[1]=(uint8_t)n;}
static void be32(uint8_t *p,unsigned n){p[0]=(uint8_t)(n>>24);p[1]=(uint8_t)(n>>16);p[2]=(uint8_t)(n>>8);p[3]=(uint8_t)n;}
int rc_easyflash_sync(rc_easyflash *e){
    if(!e->port||!e->storage)return 0;
    uint64_t masks[2]={e->flash[0].dirty_banks,e->flash[1].dirty_banks};if(!(masks[0]|masks[1])){e->sync_failed=0;return 1;}
    rc_crt *crt=&e->port->crt;rc_media *m=crt->image;if(!m||!m->writable){e->sync_failed=1;return 0;}
    size_t missing=0;
    for(unsigned which=0;which<2;++which)for(unsigned bank=0;bank<64;++bank)if((masks[which]>>bank)&1u)if(!bank_chip(crt,which,bank))++missing;
    /* Prepare both allocations before changing the borrowed media. */
    rc_crt_chip *descriptors=crt->chips;uint8_t *bytes=m->data;size_t size=m->size,count=crt->count;
    if(missing){
        if(size>RC_MEDIA_LIMIT-missing*0x2010u){e->sync_failed=1;return 0;}
        descriptors=malloc((count+missing)*sizeof *descriptors);bytes=malloc(size+missing*0x2010u);
        if(!descriptors||!bytes){free(descriptors);free(bytes);e->sync_failed=1;return 0;}
        memcpy(descriptors,crt->chips,count*sizeof *descriptors);memcpy(bytes,m->data,size);
    }
    for(unsigned which=0;which<2;++which)for(unsigned bank=0;bank<64;++bank)if((masks[which]>>bank)&1u){
        const rc_crt_chip *ch=bank_chip(crt,which,bank);size_t offset;
        if(ch)offset=ch->offset;
        else {
            uint8_t *p=bytes+size;memset(p,0,16);memcpy(p,"CHIP",4);be32(p+4,0x2010);be16(p+8,2);be16(p+10,bank);be16(p+12,which?0xa000:0x8000);be16(p+14,0x2000);
            offset=size+16;descriptors[count++]=(rc_crt_chip){2,(uint16_t)bank,(uint16_t)(which?0xa000:0x8000),0x2000,offset};size+=0x2010;
        }
        memcpy(bytes+offset,e->flash[which].memory+bank*0x2000u,0x2000);
    }
    if(missing){free(crt->chips);free(m->data);crt->chips=descriptors;crt->count=count;m->data=bytes;m->size=size;}
    m->dirty=1;e->flash[0].dirty_banks=e->flash[1].dirty_banks=0;e->sync_failed=0;return 1;
}
void rc_easyflash_boot(rc_easyflash *e,int enabled){e->boot=(uint8_t)(enabled!=0);}
static uint8_t mask(const void *ctx,uint16_t a,unsigned selected,int vic){(void)ctx;(void)a;(void)vic;return selected&(RC_CART_ROML|RC_CART_ROMH|RC_CART_IO2)?255:0;}
static rc_cart_lines lines(const void *ctx){const rc_easyflash *e=ctx;rc_cart_lines p={1,1,1,1,1,e->ba};p.exrom=(e->control&2u)==0;p.game=e->control&4u?(e->control&1u)==0:!e->boot;return p;}
static uint8_t read(void *ctx,uint16_t a,unsigned selected,uint8_t bus,int effects,int vic){
    rc_easyflash *e=ctx;(void)vic;if(selected&RC_CART_IO2)return e->ram[a&255u];
    if(selected&(RC_CART_ROML|RC_CART_ROMH)){unsigned which=(selected&RC_CART_ROMH)!=0;return rc_am29f040_read(&e->flash[which],e->bank*0x2000u+(a&0x1fffu),effects);}
    return bus;
}
static void write(void *ctx,uint16_t a,unsigned selected,uint8_t v){
    rc_easyflash *e=ctx;
    if(selected&RC_CART_IO1){if(!(a&2u))e->bank=v&63u;else e->control=v&0x87u;}
    if(selected&RC_CART_IO2)e->ram[a&255u]=v;
    if((selected&(RC_CART_ROML|RC_CART_ROMH))&&e->port->crt.image->writable){unsigned which=(selected&RC_CART_ROMH)!=0;rc_am29f040_write(&e->flash[which],e->bank*0x2000u+(a&0x1fffu),v);}
}
static void tick(void *ctx,uint64_t cycles,int ba){rc_easyflash *e=ctx;e->ba=(uint8_t)(ba!=0);for(unsigned i=0;i<2;++i)rc_am29f040_tick(&e->flash[i],cycles);if(e->flash[0].dirty_banks|e->flash[1].dirty_banks)rc_easyflash_sync(e);}
void rc_easyflash_save(const rc_easyflash *e,rc_state_writer *w){
    rc_sw_u32(w,1);rc_sw_u8(w,e->bank);rc_sw_u8(w,e->control);rc_sw_u8(w,e->boot);rc_sw_u8(w,e->ba);rc_sw_u8(w,(uint8_t)e->sync_failed);rc_sw_bytes(w,e->ram,sizeof e->ram);for(unsigned i=0;i<2;++i)rc_am29f040_save(&e->flash[i],w);
}
int rc_easyflash_load(rc_easyflash *e,rc_state_reader *r){
    if(!e->storage||rc_sr_u32(r)!=1)return 0;rc_easyflash n=*e;
    n.bank=rc_sr_u8(r);n.control=rc_sr_u8(r);n.boot=rc_sr_u8(r);n.ba=rc_sr_u8(r);n.sync_failed=rc_sr_u8(r);rc_sr_bytes(r,n.ram,sizeof n.ram);
    if(!rc_sr_ok(r)||n.bank>63||(n.control&~0x87u)||n.boot>1||n.ba>1||n.sync_failed>1)return 0;
    uint8_t *temporary=malloc(2u*RC_AM29F040_SIZE);if(!temporary)return 0;
    for(unsigned i=0;i<2;++i){n.flash[i].memory=temporary+i*RC_AM29F040_SIZE;if(!rc_am29f040_load(&n.flash[i],r)){free(temporary);return 0;}}
    memcpy(e->storage,temporary,2u*RC_AM29F040_SIZE);free(temporary);
    for(unsigned i=0;i<2;++i)n.flash[i].memory=e->storage+i*RC_AM29F040_SIZE;
    *e=n;return 1;
}
static void save(const void *ctx,rc_state_writer *w){rc_easyflash_save(ctx,w);}static int load(void *ctx,rc_state_reader *r){return rc_easyflash_load(ctx,r);}
static const rc_cart_ops *mapper_ops(void){static const rc_cart_ops ops={.drive_mask=mask,.reset=reset,.read=read,.write=write,.lines=lines,.tick=tick,.save=save,.load=load};return &ops;}
