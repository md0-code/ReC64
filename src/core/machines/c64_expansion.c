/* Expansion connector, interrupt combining and bus grants: C64 schematics. */
#include "c64_expansion.h"
#include "../media/image.h"
#include "../../host/host.h"
#include <stdio.h>
#include <string.h>
static unsigned selected(uint16_t a,unsigned pla,int vic){
    unsigned s=(pla&RC_PLA_ROML?RC_CART_ROML:0)|(pla&RC_PLA_ROMH?RC_CART_ROMH:0);
    if(!vic&&(pla&RC_PLA_I_O)){if((a&0xff00u)==0xde00u)s|=RC_CART_IO1;if((a&0xff00u)==0xdf00u)s|=RC_CART_IO2;}return s;
}
/* Nothing in the port: its lines rest, and every interrupt source reports its own changes. */
static int empty(const rc_c64 *c){return !c->cart.crt.image&&!c->cart.ops&&!c->cart.capacitor&&!c->reu.size&&!c->georam.size&&!c->freeze_cycles;}
void rc_c64_expansion_lines(rc_c64 *c){
    const rc_cart_lines p=rc_cart_get_lines(&c->cart);c->bus.game=p.game;c->bus.exrom=p.exrom;
    rc_mos6510_input(&c->cpu,RC_CPU_IRQ,c->vic.irq||c->cia[0].irq||!p.irq||rc_reu_irq(&c->reu));
    rc_mos6510_input(&c->cpu,RC_CPU_NMI,c->restore||c->cia[1].irq||!p.nmi);
}
static void observe(void *ctx,uint16_t a,int write,int effects,int vic){
    rc_c64 *c=ctx;if(empty(c))return;if(!c->bus.dma||c->gmod2.storage)rc_cart_observe(&c->cart,a,write,effects,vic);rc_c64_expansion_lines(c);
    /* A debugger read decodes read-cycle pins, then the bus restores its
     * physical levels. The mapper's last real write cycle is untouched. */
    if(!effects&&c->gmod2.storage){c->bus.game=1;c->bus.exrom=(c->gmod2.control>>6)&1u;}
}
static uint8_t read_bus(void *ctx,uint16_t a,unsigned pla,uint8_t data,int driven,int effects,int vic){
    rc_c64 *c=ctx;if(empty(c))return driven?data:c->bus.vic_data;
    const unsigned s=selected(a,pla,vic);uint8_t mask=rc_cart_drive_mask(&c->cart,a,s,vic);
    const uint8_t value=rc_cart_read(&c->cart,a,s,c->bus.vic_data,effects,vic);
    if(mask){data=(uint8_t)((driven?data:255)&(value|~mask));driven=1;}
    if(!vic&&(s&RC_CART_IO2)&&!c->bus.dma){mask=rc_reu_drive_mask(&c->reu,a);const uint8_t v=rc_reu_read(&c->reu,a,c->bus.vic_data,effects);if(mask){data=(uint8_t)((driven?data:255)&(v|~mask));driven=1;}}
    if(!vic&&(s&RC_CART_IO1)){mask=rc_georam_drive_mask(&c->georam,a);const uint8_t v=rc_georam_read(&c->georam,a,c->bus.vic_data,effects);if(mask){data=(uint8_t)((driven?data:255)&(v|~mask));driven=1;}}
    if(effects)rc_c64_expansion_lines(c);return driven?data:c->bus.vic_data;
}
static void write_bus(void *ctx,uint16_t a,unsigned pla,uint8_t v){
    rc_c64 *c=ctx;if(empty(c))return;const unsigned s=selected(a,pla,0);rc_cart_write(&c->cart,a,s,v);
    if((s&RC_CART_IO2)&&!c->bus.dma)rc_reu_write(&c->reu,a,v);
    if(s&(RC_CART_IO1|RC_CART_IO2))rc_georam_write(&c->georam,a,v);
    if(!c->bus.dma)rc_reu_observe_write(&c->reu,a);rc_c64_expansion_lines(c);
}
int rc_c64_expansion_init(rc_c64 *c,const rc_machine_setup *setup,char *error,size_t cap){
    static const uint32_t reu[]={0,0x20000,0x40000,0x80000,0x100000,0x200000,0x400000,0x800000,0x1000000};
    static const uint32_t geo[]={0,0x80000,0x100000,0x200000,0x400000};
    if(setup->options[2]>=sizeof reu/sizeof reu[0]||setup->options[3]>=sizeof geo/sizeof geo[0]||(setup->options[2]&&setup->options[3]))return rc_image_error(error,cap,"Select either REU or GeoRAM");
    rc_cart_init(&c->cart);rc_easyflash_init(&c->easyflash);rc_gmod2_init(&c->gmod2);
    if(!rc_reu_init(&c->reu,reu[setup->options[2]])||!rc_georam_init(&c->georam,geo[setup->options[3]]))return rc_image_error(error,cap,"Cannot allocate expansion RAM");
    c->bus.expansion=(rc_bus_expansion){c,observe,read_bus,write_bus};return 1;
}
void rc_c64_expansion_release(rc_c64 *c){rc_easyflash_close(&c->easyflash);rc_gmod2_close(&c->gmod2);rc_media_close(c->cart_eeprom);c->cart_eeprom=NULL;rc_cart_close(&c->cart);rc_reu_free(&c->reu);rc_georam_free(&c->georam);}
void rc_c64_expansion_reset(rc_c64 *c){c->freeze_cycles=0;rc_cart_reset(&c->cart);rc_reu_reset(&c->reu);rc_georam_reset(&c->georam);rc_c64_expansion_lines(c);}
void rc_c64_expansion_tick(rc_c64 *c){
    if(empty(c)){c->cart.ba=(uint8_t)(c->vic.ba!=0);return;}
    if(c->freeze_cycles&&!--c->freeze_cycles)rc_cart_freeze(&c->cart,0);
    rc_cart_tick(&c->cart,1,c->vic.ba);rc_c64_expansion_lines(c);
}
int rc_c64_cart_flush(rc_c64 *c){
    if(!c->cart.crt.image)return 1;
    if(c->cart.crt.hardware==RC_CRT_EASYFLASH)return rc_easyflash_sync(&c->easyflash);
    if(c->cart.crt.hardware!=RC_CRT_GMOD2)return 1;
    if(!rc_gmod2_sync(&c->gmod2))return 0;
    rc_media *e=c->cart_eeprom;if(!e)return 1;
    if(memcmp(e->data,c->gmod2.eeprom.memory,RC_M93C86_SIZE)){memcpy(e->data,c->gmod2.eeprom.memory,RC_M93C86_SIZE);e->dirty=1;}
    return rc_media_save(e);
}
static int eeprom_open(rc_gmod2 *g,rc_media **out,char *error,size_t cap){
    const char *image=g->port->crt.image->path;*out=NULL;if(!*image)return 1;
    char path[RC_MEDIA_PATH];if(snprintf(path,sizeof path,"%s.eeprom",image)>=(int)sizeof path)return rc_image_error(error,cap,"EEPROM path is too long");
    uint64_t size;
    if(rc_host_file_size(path,&size)){
        if(size!=RC_M93C86_SIZE)return rc_image_error(error,cap,"GMod2 EEPROM must contain 2048 bytes");
        *out=rc_media_open(path,0,error,cap);if(!*out)return 0;
        if((*out)->size!=RC_M93C86_SIZE){rc_media_close(*out);*out=NULL;return rc_image_error(error,cap,"GMod2 EEPROM must contain 2048 bytes");}memcpy(g->eeprom.memory,(*out)->data,RC_M93C86_SIZE);
    }else {
        if(rc_host_folder_exists(path))return rc_image_error(error,cap,"GMod2 EEPROM path is a folder");
        *out=rc_media_from(path,g->eeprom.memory,RC_M93C86_SIZE);if(!*out)return rc_image_error(error,cap,"Out of memory");(*out)->writable=1;
    }
    return 1;
}
int rc_c64_cart_insert(rc_c64 *c,rc_media *image,char *error,size_t cap){
    rc_cart next;rc_easyflash flash;rc_freezer freezer;rc_gmod2 gmod;rc_media *eeprom=NULL;rc_cart_init(&next);rc_easyflash_init(&flash);rc_gmod2_init(&gmod);memset(&freezer,0,sizeof freezer);
    if(image){
        if(!rc_cart_attach(&next,image,error,cap))return 0;const unsigned h=next.crt.hardware;
        if(h==RC_CRT_EASYFLASH){if(!rc_easyflash_attach(&flash,&next,c->hz,error,cap)){rc_cart_close(&next);return 0;}}
        else if(h==RC_CRT_GMOD2){if(!rc_gmod2_attach(&gmod,&next,c->hz,error,cap)||!eeprom_open(&gmod,&eeprom,error,cap)){rc_gmod2_close(&gmod);rc_cart_close(&next);return 0;}}
        else if(h==RC_CRT_RGCD){
            int valid=next.crt.revision<=1&&next.crt.count<=8&&next.crt.count&&(next.crt.count&(next.crt.count-1))==0;
            if(!next.crt.revision&&next.crt.count!=8)valid=0;
            for(size_t i=0;i<next.crt.count;++i){const rc_crt_chip *ch=&next.crt.chips[i];if(ch->bank>=next.crt.count||ch->address!=0x8000||ch->size!=0x2000||ch->type==1)valid=0;}
            if(!valid){rc_cart_close(&next);return rc_image_error(error,cap,"Invalid RGCD CHIP geometry or revision");}
        }
        else if(rc_freezer_supported(h)){if(!rc_freezer_init(&freezer,&next.crt)){rc_cart_close(&next);return rc_image_error(error,cap,"Invalid freezer CRT");}rc_cart_bind(&next,&rc_freezer_ops,&freezer);}
        else if(!rc_cart_supported(h)){rc_cart_close(&next);return rc_image_error(error,cap,"Unsupported CRT hardware");}
    }
    rc_easyflash_close(&c->easyflash);rc_gmod2_close(&c->gmod2);rc_media_close(c->cart_eeprom);rc_cart_close(&c->cart);c->freeze_cycles=0;c->cart=next;c->easyflash=flash;c->freezer=freezer;c->gmod2=gmod;c->cart_eeprom=eeprom;
    if(c->cart.ops==&rc_freezer_ops){c->freezer.crt=&c->cart.crt;c->cart.ctx=&c->freezer;}
    else if(c->easyflash.storage){c->easyflash.port=&c->cart;c->cart.ctx=&c->easyflash;}
    else if(c->gmod2.storage){c->gmod2.port=&c->cart;c->cart.ctx=&c->gmod2;}
    rc_c64_expansion_lines(c);return 1;
}
int rc_c64_cart_freeze(rc_c64 *c,uint32_t value){
    const int enabled=c->cart.crt.image&&c->cart.ops&&c->cart.ops->freeze;
    if(value==UINT32_MAX)return enabled;if(!enabled||c->freeze_cycles)return 0;
    rc_cart_freeze(&c->cart,1);c->freeze_cycles=(c->hz+999u)/1000u;rc_c64_expansion_lines(c);return 1;
}
void rc_c64_expansion_save(const rc_c64 *c,rc_state_writer *w){
    rc_sw_begin(w,"CART",0);rc_sw_u32(w,1);rc_sw_u32(w,c->freeze_cycles);rc_cart_save(&c->cart,w);rc_sw_end(w);
    rc_sw_begin(w,"REU ",0);rc_reu_save(&c->reu,w);rc_sw_end(w);
    rc_sw_begin(w,"GEO ",0);rc_georam_save(&c->georam,w);rc_sw_end(w);
}
int rc_c64_expansion_load(rc_c64 *c,rc_state_reader *r){
    if(!rc_sr_chunk(r,"CART",0))return !c->media[3]&&!c->reu.size&&!c->georam.size;
    char error[128];if(!rc_c64_cart_insert(c,c->media[3],error,sizeof error)||rc_sr_u32(r)!=1)return 0;
    c->freeze_cycles=rc_sr_u32(r);if(c->freeze_cycles>(c->hz+999u)/1000u||!rc_cart_load(&c->cart,r))return 0;
    const uint32_t reu_size=c->reu.size,geo_size=c->georam.size;
    if(!rc_sr_chunk(r,"REU ",0)||!rc_reu_load(&c->reu,r)||c->reu.size!=reu_size)return 0;
    if(!rc_sr_chunk(r,"GEO ",0)||!rc_georam_load(&c->georam,r)||c->georam.size!=geo_size)return 0;
    return 1;
}
