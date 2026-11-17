/* Native bus fixture for RGCD and GMod2, from their published hardware wiring. */
#include "../core/machines/c64_expansion.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define CHECK(x) do{if(!(x)){fprintf(stderr,"line %d: %s\n",__LINE__,#x);return 1;}}while(0)
static void be16(uint8_t *p,unsigned v){p[0]=(uint8_t)(v>>8);p[1]=(uint8_t)v;}
static void be32(uint8_t *p,unsigned v){p[0]=(uint8_t)(v>>24);p[1]=(uint8_t)(v>>16);p[2]=(uint8_t)(v>>8);p[3]=(uint8_t)v;}
static rc_media *fixture(const char *path,unsigned hardware){
    const unsigned count=hardware==RC_CRT_RGCD?8u:2u;const size_t size=64+count*0x2010u;uint8_t *bytes=calloc(size,1);if(!bytes)return NULL;
    memcpy(bytes,"C64 CARTRIDGE   ",16);be32(bytes+16,64);be16(bytes+20,0x100);be16(bytes+22,hardware);bytes[25]=1;
    for(unsigned i=0;i<count;++i){const unsigned b=hardware==RC_CRT_GMOD2&&i?63:i;uint8_t *p=bytes+64+i*0x2010u;memcpy(p,"CHIP",4);be32(p+4,0x2010);be16(p+8,hardware==RC_CRT_GMOD2?2:0);be16(p+10,b);be16(p+12,0x8000);be16(p+14,0x2000);memset(p+16,255,0x2000);p[16]=(uint8_t)b;}
    rc_media *m=rc_media_from(path,bytes,size);free(bytes);if(m)m->writable=1;return m;
}
static void reg(rc_c64 *c,uint8_t v){rc_bus_write(&c->bus,0xde00,v);}
static void bit(rc_c64 *c,unsigned v){reg(c,(uint8_t)(0x40|(v?0x10:0)));reg(c,(uint8_t)(0x60|(v?0x10:0)));reg(c,(uint8_t)(0x40|(v?0x10:0)));}
static void send(rc_c64 *c,unsigned v,unsigned bits){while(bits)bit(c,(v>>--bits)&1u);}
static void command(rc_c64 *c,unsigned op,unsigned address){reg(c,0);send(c,0x1000u|(op<<10)|address,13);}
static unsigned word(rc_c64 *c,unsigned address){command(c,2,address);unsigned value=0;for(unsigned i=0;i<16;++i){bit(c,0);value=(value<<1)|(rc_bus_read(&c->bus,0xde00,1)>>7);}reg(c,0);return value;}
static int snapshot(rc_c64 *c,rc_state_writer *w){rc_sw_init(w);rc_sw_header(w,"c64","c64-pal",NULL,0);rc_sw_begin(w,"CART",0);rc_cart_save(&c->cart,w);rc_sw_end(w);return rc_sw_finish(w);}
static int restore(rc_c64 *c,const rc_state_writer *w){rc_state_reader r;char error[128];return rc_sr_open(&r,w->data,w->size,error,sizeof error)&&rc_sr_chunk(&r,"CART",0)&&rc_cart_load(&c->cart,&r);}
int main(void){
    char error[256],path[RC_MEDIA_PATH];rc_rom_index roms;rc_rom_index_init(&roms);const char *folders[]={RC_ROM_DIR};CHECK(rc_rom_index_scan(&roms,folders,1));
    const unsigned options[]={0,0,0,0,0};rc_machine_setup setup={.family=&rc_c64_family,.model=0,.options=options,.roms=&roms};rc_c64 *c=rc_c64_create(&setup,error,sizeof error);CHECK(c);c->vic.ba=1;rc_bus_write(&c->bus,0,0x2f);rc_bus_write(&c->bus,1,0x37);
    rc_media *rgcd=fixture("",RC_CRT_RGCD);CHECK(rgcd&&rc_c64_cart_insert(c,rgcd,error,sizeof error));
    reg(c,7);CHECK(rc_bus_read(&c->bus,0x8000,1)==7);rc_bus_write(&c->bus,0xde7f,4);CHECK(rc_bus_read(&c->bus,0x8000,1)==4);
    reg(c,8);reg(c,1);CHECK(rc_cart_get_lines(&c->cart).exrom);rc_state_writer locked;CHECK(snapshot(c,&locked));rc_cart_reset(&c->cart);CHECK(!rc_cart_get_lines(&c->cart).exrom);CHECK(restore(c,&locked)&&rc_cart_get_lines(&c->cart).exrom);rc_sw_free(&locked);
    CHECK(rc_c64_cart_insert(c,NULL,error,sizeof error));rgcd->data[26]=1;CHECK(rc_c64_cart_insert(c,rgcd,error,sizeof error));CHECK(rc_bus_read(&c->bus,0x8000,1)==7);reg(c,7);CHECK(rc_bus_read(&c->bus,0x8000,1)==0);CHECK(rc_c64_cart_insert(c,NULL,error,sizeof error));rc_media_close(rgcd);
    snprintf(path,sizeof path,"%s/gmod2-written.crt",RC_WORK);char eeprom_path[RC_MEDIA_PATH];snprintf(eeprom_path,sizeof eeprom_path,"%s.eeprom",path);remove(eeprom_path);rc_media *m=fixture(path,RC_CRT_GMOD2);CHECK(m);m->dirty=1;CHECK(rc_media_save(m)&&rc_c64_cart_insert(c,m,error,sizeof error));
    CHECK(rc_bus_read(&c->bus,0x8000,1)==0);reg(c,63);CHECK(rc_bus_read(&c->bus,0x8000,1)==63);CHECK(word(c,5)==65535);
    command(c,0,0x300);reg(c,0); /* Write enable. */
    command(c,1,5);send(c,0xbe,8);rc_state_writer partial,expected,actual;CHECK(snapshot(c,&partial));
    send(c,0xef,8);reg(c,0);rc_cart_tick(&c->cart,2000,1);CHECK(c->gmod2.eeprom.remaining);CHECK(snapshot(c,&expected));
    CHECK(restore(c,&partial));send(c,0xef,8);reg(c,0);rc_cart_tick(&c->cart,2000,1);CHECK(snapshot(c,&actual));CHECK(expected.size==actual.size&&!memcmp(expected.data,actual.data,actual.size));rc_sw_free(&partial);rc_sw_free(&expected);rc_sw_free(&actual);
    reg(c,0x40);CHECK(!(rc_bus_read(&c->bus,0xde00,1)&128u));CHECK(snapshot(c,&partial));rc_cart_tick(&c->cart,3000,1);CHECK(rc_bus_read(&c->bus,0xde00,1)&128u);CHECK(snapshot(c,&expected));
    CHECK(restore(c,&partial));rc_cart_tick(&c->cart,3000,1);CHECK(snapshot(c,&actual));CHECK(expected.size==actual.size&&!memcmp(expected.data,actual.data,actual.size));rc_sw_free(&partial);rc_sw_free(&expected);rc_sw_free(&actual);reg(c,0);CHECK(word(c,5)==0xbeef);
    command(c,0,0);reg(c,0);command(c,1,5);send(c,0x1234,16);reg(c,0);rc_cart_tick(&c->cart,5000,1);CHECK(word(c,5)==0xbeef); /* Write disabled. */
    reg(c,0xc0|17);c->bus.ram[0xe123]=0x99;c->bus.ram[0x8123]=0x88;rc_bus_write(&c->bus,0x8123,0x44);CHECK(c->bus.ram[0x8123]==0x88);
    rc_bus_write(&c->bus,0xe555,0xaa);rc_bus_write(&c->bus,0xe2aa,0x55);rc_bus_write(&c->bus,0xe555,0xa0);rc_bus_dma_write(&c->bus,0xe123,0x5a);CHECK(c->gmod2.flash.operation&&c->bus.ram[0xe123]==0x99);
    CHECK(snapshot(c,&partial));const uint8_t game=c->bus.game,exrom=c->bus.exrom;CHECK(rc_bus_read(&c->bus,0xe123,0)==c->bus.kernal[0x123]&&c->bus.game==game&&c->bus.exrom==exrom);CHECK(snapshot(c,&actual));CHECK(partial.size==actual.size&&!memcmp(partial.data,actual.data,actual.size));rc_sw_free(&partial);rc_sw_free(&actual);
    rc_cart_tick(&c->cart,7,1);reg(c,17);CHECK(rc_bus_read(&c->bus,0x8123,1)==0x5a);
    CHECK(rc_c64_cart_flush(c)&&rc_media_save(m));rc_media *saved=rc_media_open(eeprom_path,0,error,sizeof error);CHECK(saved&&saved->size==2048&&saved->data[10]==0xbe&&saved->data[11]==0xef);rc_media_close(saved);
    CHECK(rc_c64_cart_insert(c,NULL,error,sizeof error));rc_media_close(m);m=rc_media_open(path,0,error,sizeof error);CHECK(m&&rc_c64_cart_insert(c,m,error,sizeof error));CHECK(word(c,5)==0xbeef);reg(c,17);CHECK(rc_bus_read(&c->bus,0x8123,1)==0x5a);
    CHECK(rc_c64_cart_insert(c,NULL,error,sizeof error));rc_media_close(m);rc_c64_family.impl->destroy(c);rc_rom_index_free(&roms);CHECK(remove(path)==0&&remove(eeprom_path)==0);
    puts("RGCD banking/disable/revision, GMod2 flash/bus/EEPROM, exact active state and persistent saves passed");return 0;
}
