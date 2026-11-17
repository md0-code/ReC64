#include "cart.h"
#include <stdlib.h>
#include <string.h>
void rc_cart_init(rc_cart *c){memset(c,0,sizeof *c);c->ba=1;}
void rc_cart_close(rc_cart *c){rc_crt_close(&c->crt);rc_cart_init(c);}
int rc_cart_supported(unsigned h){switch(h){case RC_CRT_NORMAL:case RC_CRT_EPYX:case RC_CRT_SIMONS_BASIC:case RC_CRT_OCEAN:case RC_CRT_FUN_PLAY:case RC_CRT_SUPER_GAMES:case RC_CRT_SYSTEM3:case RC_CRT_DINAMIC:case RC_CRT_ZAXXON:case RC_CRT_MAGIC_DESK:case RC_CRT_RGCD:return 1;default:return 0;}}
int rc_cart_attach(rc_cart *c,rc_media *m,char *error,size_t cap){
    if(!rc_crt_attach(&c->crt,m,error,cap))return 0;c->ops=NULL;c->ctx=NULL;rc_cart_reset(c);return 1;
}
void rc_cart_bind(rc_cart *c,const rc_cart_ops *ops,void *ctx){c->ops=ops;c->ctx=ctx;rc_cart_reset(c);}
void rc_cart_reset(rc_cart *c){c->bank=0;c->control=0;c->locked=0;c->ba=1;c->capacitor=c->crt.hardware==RC_CRT_EPYX?512:0;if(c->ops&&c->ops->reset)c->ops->reset(c->ctx);}
void rc_cart_observe(rc_cart *c,uint16_t a,int write,int effects,int vic){if(c->ops&&c->ops->observe)c->ops->observe(c->ctx,a,write,effects,vic);}
rc_cart_lines rc_cart_get_lines(const rc_cart *c){
    rc_cart_lines p={1,1,1,1,1,c->ba};if(c->ops&&c->ops->lines){p=c->ops->lines(c->ctx);p.ba=c->ba;return p;}
    if(!c->crt.image||!rc_cart_supported(c->crt.hardware))return p;
    p.game=c->crt.game;p.exrom=c->crt.exrom;
    switch(c->crt.hardware){
    case RC_CRT_EPYX:p.game=1;p.exrom=c->capacitor==0;break;
    case RC_CRT_SIMONS_BASIC:p.game=c->control&1u;p.exrom=0;break;
    case RC_CRT_FUN_PLAY:p.game=c->control==0x86;p.exrom=p.game;break;
    case RC_CRT_SUPER_GAMES:p.game=(c->control>>2)&1u;p.exrom=p.game;break;
    case RC_CRT_MAGIC_DESK:p.game=1;p.exrom=c->control>>7;break;
    case RC_CRT_RGCD:p.game=1;p.exrom=c->locked;break;
    case RC_CRT_ZAXXON:p.game=0;p.exrom=0;break;
    case RC_CRT_SYSTEM3:case RC_CRT_DINAMIC:p.game=1;p.exrom=0;break;
    default:break;
    }return p;
}
static const rc_crt_chip *read_chip(const rc_cart *c,uint16_t a,unsigned sel,unsigned *address){
    if(c->crt.hardware==RC_CRT_EPYX&&(sel&RC_CART_IO2)){*address=0x9f00u+(a&255u);return rc_crt_find(&c->crt,0,*address);}
    if(!c->crt.image||!(sel&(RC_CART_ROML|RC_CART_ROMH))||!rc_cart_supported(c->crt.hardware))return NULL;
    unsigned bank=c->bank;*address=(sel&RC_CART_ROMH?0xa000u:0x8000u)+(a&0x1fffu);
    if(c->crt.hardware==RC_CRT_RGCD&&c->crt.revision==1){unsigned last=0;for(size_t i=0;i<c->crt.count;++i)if(c->crt.chips[i].bank>last)last=c->crt.chips[i].bank;bank=(7u-bank)&last;}
    if(c->crt.hardware==RC_CRT_NORMAL){bank=0;*address=a;}
    if(c->crt.hardware==RC_CRT_ZAXXON){if(sel&RC_CART_ROML){bank=0;*address=0x8000u+(a&0xfffu);}}
    if(c->crt.hardware==RC_CRT_OCEAN){
    /* 256K images describe the two 128K ROM halves as bank 0..15 at
     * $8000 and bank 16..31 at $A000. Both halves share A13..A16. */
    int split=0;for(size_t i=0;i<c->crt.count;++i)if(c->crt.chips[i].address==0xa000&&c->crt.chips[i].bank>=16){split=1;break;}
    if(split)bank=(bank&15u)+(sel&RC_CART_ROMH?16u:0u);
    const rc_crt_chip *ch=rc_crt_find(&c->crt,bank,*address);
    if(ch)return ch;
    /* Single 8K ROM banks drive either selected ROM window. */
    *address=0x8000u+(a&0x1fffu);ch=rc_crt_find(&c->crt,bank,*address);
    if(!ch){*address=0xa000u+(a&0x1fffu);ch=rc_crt_find(&c->crt,bank,*address);}return ch;
    }
    const rc_crt_chip *ch=rc_crt_find(&c->crt,bank,*address);
    if(!ch&&c->crt.hardware==RC_CRT_FUN_PLAY&&(sel&RC_CART_ROMH)){*address=0x8000u+(a&0x1fffu);ch=rc_crt_find(&c->crt,bank,*address);}
    return ch;
}
uint8_t rc_cart_drive_mask(const rc_cart *c,uint16_t a,unsigned sel,int vic){
    if(c->ops)return c->ops->drive_mask?c->ops->drive_mask(c->ctx,a,sel,vic):0;
    unsigned address;const rc_crt_chip *ch=read_chip(c,a,sel,&address);return ch&&ch->type!=1&&ch->offset!=SIZE_MAX?255:0;
}
uint8_t rc_cart_read(rc_cart *c,uint16_t a,unsigned sel,uint8_t bus,int effects,int vic){
    if(c->ops&&c->ops->read)return c->ops->read(c->ctx,a,sel,bus,effects,vic);
    if(!c->crt.image||!rc_cart_supported(c->crt.hardware))return bus;
    if(effects&&!vic){
        if(c->crt.hardware==RC_CRT_EPYX&&(sel&(RC_CART_ROML|RC_CART_IO1)))c->capacitor=512;
        if((sel&RC_CART_IO1)&&c->crt.hardware==RC_CRT_DINAMIC)c->bank=a&15u;
        if((sel&RC_CART_IO1)&&c->crt.hardware==RC_CRT_SYSTEM3)c->bank=0;
        if((sel&RC_CART_IO1)&&a==0xde00&&c->crt.hardware==RC_CRT_SIMONS_BASIC)c->control=1;
        if((sel&RC_CART_ROML)&&c->crt.hardware==RC_CRT_ZAXXON)c->bank=(a>>12)&1u;
    }
    unsigned address;const rc_crt_chip *ch=read_chip(c,a,sel,&address);
    return ch&&ch->type!=1&&ch->offset!=SIZE_MAX?c->crt.image->data[ch->offset+address-ch->address]:bus;
}
void rc_cart_write(rc_cart *c,uint16_t a,unsigned sel,uint8_t v){
    if(c->ops&&c->ops->write){c->ops->write(c->ctx,a,sel,v);return;}
    if(!c->crt.image)return;
    switch(c->crt.hardware){
    case RC_CRT_SIMONS_BASIC:if((sel&RC_CART_IO1)&&a==0xde00)c->control=0;break;
    case RC_CRT_OCEAN:if(sel&RC_CART_IO1){c->bank=v&63u;c->control=v;}break;
    case RC_CRT_MAGIC_DESK:if(sel&RC_CART_IO1){c->bank=v&127u;c->control=v;}break;
    case RC_CRT_RGCD:if((sel&RC_CART_IO1)&&!c->locked){c->bank=v&7u;c->control=v&15u;c->locked=(v>>3)&1u;}break;
    /* Fun Play CRT bank numbers store the literal latch bit pattern. */
    case RC_CRT_FUN_PLAY:if(sel&RC_CART_IO1){c->bank=v&0x39u;c->control=v;}break;
    case RC_CRT_SUPER_GAMES:if((sel&RC_CART_IO2)&&!c->locked){c->bank=v&3u;c->control=v&15u;c->locked=(v>>3)&1u;}break;
    case RC_CRT_SYSTEM3:if(sel&RC_CART_IO1)c->bank=a&63u;break;
    default:break;
    }
}
void rc_cart_tick(rc_cart *c,uint64_t cycles,int ba){c->ba=(uint8_t)(ba!=0);if(c->capacitor)c->capacitor=cycles>=c->capacitor?0:(uint16_t)(c->capacitor-cycles);if(c->ops&&c->ops->tick)c->ops->tick(c->ctx,cycles,ba);}
void rc_cart_freeze(rc_cart *c,int pressed){if(c->ops&&c->ops->freeze)c->ops->freeze(c->ctx,pressed);}
void rc_cart_save(const rc_cart *c,rc_state_writer *w){
    rc_sw_u32(w,2);rc_sw_u16(w,c->bank);rc_sw_u8(w,c->control);rc_sw_u8(w,c->locked);rc_sw_u8(w,c->ba);rc_sw_u8(w,c->ops!=NULL);rc_sw_u16(w,c->capacitor);rc_crt_save(&c->crt,w);
    if(c->ops){if(c->ops->save)c->ops->save(c->ctx,w);else w->failed=1;}
}
int rc_cart_load(rc_cart *c,rc_state_reader *r){
    unsigned version=rc_sr_u32(r);if(version<1||version>2)return 0;uint16_t bank=rc_sr_u16(r);uint8_t control=rc_sr_u8(r),locked=rc_sr_u8(r),ba=rc_sr_u8(r),external=rc_sr_u8(r);
    uint16_t capacitor=version>=2?rc_sr_u16(r):0;
    if(!rc_sr_ok(r)||capacitor>512||locked>1||ba>1||external>1||external!=(c->ops!=NULL))return 0;
    rc_crt next;rc_crt_init(&next);if(!rc_crt_load(&next,r))return 0;
    if(external&&(!c->crt.image||!next.image||next.hardware!=c->crt.hardware||!c->ops->load||!c->ops->load(c->ctx,r))){rc_crt_close(&next);return 0;}
    if(c->crt.image&&!c->crt.owned&&next.image){
        rc_media *m=c->crt.image;
        free(m->data);m->data=next.image->data;m->size=next.image->size;m->dirty=next.image->dirty;next.image->data=NULL;rc_media_close(next.image);next.image=m;next.owned=0;
    }
    rc_crt_close(&c->crt);c->crt=next;c->bank=bank;c->control=control;c->locked=locked;c->ba=ba;c->capacitor=capacitor;return rc_sr_ok(r);
}
