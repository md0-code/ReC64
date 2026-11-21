#include "../src/hw/easyflash.h"
#include "../src/core/font8x8.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define CHECK(x) do{if(!(x)){fprintf(stderr,"line %d: %s\n",__LINE__,#x);return 1;}}while(0)
static void put16(uint8_t *p,unsigned v){p[0]=(uint8_t)(v>>8);p[1]=(uint8_t)v;}
static void put32(uint8_t *p,unsigned v){p[0]=(uint8_t)(v>>24);p[1]=(uint8_t)(v>>16);p[2]=(uint8_t)(v>>8);p[3]=(uint8_t)v;}
static void boot_firmware(uint8_t *rom){
    /* Original 6510 firmware: bootstrap copies its flash writer into $0200
     * RAM, programs a missing ROML bank, polls it and displays the result. */
    uint8_t bootstrap[]={0x78,0xd8,0xa2,0xff,0x9a,0xa9,0x37,0x85,0x01,0xa9,0x2f,0x85,0x00,0xa2,0x00,0xbd,0x00,0xe2,0x9d,0x00,0x02,0xe8,0xe0,0x00,0xd0,0xf5,0x4c,0x00,0x02};
    uint8_t writer[]={
        0xa9,0x7f,0x8d,0x0d,0xdc,0x8d,0x0d,0xdd,
        0xa9,0x03,0x8d,0x02,0xdd,0x8d,0x00,0xdd,
        0xa9,0x1b,0x8d,0x11,0xd0,0xa9,0x08,0x8d,0x16,0xd0,0xa9,0x1e,0x8d,0x18,0xd0,
        0xa9,0x06,0x8d,0x20,0xd0,0xa9,0x00,0x8d,0x21,0xd0,
        0xa2,0x00,0xa9,0x20,0x9d,0x00,0x04,0x9d,0x00,0x05,0x9d,0x00,0x06,0x9d,0x00,0x07,0xe8,0xd0,0xf1,
        0xa9,0x01,0x9d,0x00,0xd8,0x9d,0x00,0xd9,0x9d,0x00,0xda,0x9d,0x00,0xdb,0xe8,0xd0,0xf1,
        0xa9,0x11,0x8d,0x00,0xde,
        0xa9,0xaa,0x8d,0x55,0x85,0xa9,0x55,0x8d,0xaa,0x82,0xa9,0xa0,0x8d,0x55,0x85,0xa9,0x5a,0x8d,0x23,0x81,
        0xad,0x23,0x81,0x30,0xfb,0x8d,0xff,0x02,
        0xa9,0x00,0x8d,0x00,0xde,
        0xa2,0x00,0xbd,0x00,0xe3,0xf0,0x06,0x9d,0x00,0x04,0xe8,0xd0,0xf5,
        0x4c,0x00,0x00
    };
    bootstrap[23]=(uint8_t)sizeof writer;unsigned stop=0x200+(unsigned)sizeof writer-3;writer[sizeof writer-2]=(uint8_t)stop;writer[sizeof writer-1]=(uint8_t)(stop>>8);
    memcpy(rom+0x100,bootstrap,sizeof bootstrap);memcpy(rom+0x200,writer,sizeof writer);
    const char *text="EASYFLASH WRITE PASSED";for(size_t i=0;i<=strlen(text);++i){uint8_t c=(uint8_t)text[i];rom[0x300+i]=(c>='A'&&c<='Z')?(uint8_t)(c-64):c;}
    for(unsigned c=0;c<256;++c){unsigned ascii=c<32?c+64:c;memset(rom+0x1800+c*8,0,8);if(ascii>=32&&ascii<128)memcpy(rom+0x1800+c*8,rc_font8x8_ascii[ascii-32],8);}
    for(unsigned off=0x1ffa;off<0x2000;off+=2){rom[off]=0;rom[off+1]=0xe1;}
}
static rc_media *fixture(const char *path){
    uint8_t data[64+16+8192];memset(data,0,sizeof data);memcpy(data,"C64 CARTRIDGE   ",16);put32(data+16,64);put16(data+20,0x100);put16(data+22,32);data[24]=1;memcpy(data+32,"ORIGINAL EASYFLASH TEST",23);
    memcpy(data+64,"CHIP",4);put32(data+68,0x2010);put16(data+72,2);put16(data+76,0xa000);put16(data+78,0x2000);memset(data+80,255,8192);
    boot_firmware(data+80);rc_media *m=rc_media_from(path,data,sizeof data);if(m)m->writable=1;return m;
}
static void bank(rc_cart *c,unsigned b){rc_cart_write(c,0xde00,RC_CART_IO1,(uint8_t)b);}
static void command(rc_cart *c,unsigned which,uint8_t v){unsigned select=which?RC_CART_ROMH:RC_CART_ROML;uint16_t base=which?0xe000:0x8000;rc_cart_write(c,base+0x555,select,0xaa);rc_cart_write(c,base+0x2aa,select,0x55);rc_cart_write(c,base+0x555,select,v);}
static void program(rc_cart *c,unsigned which,unsigned b,unsigned off,uint8_t v){bank(c,b);command(c,which,0xa0);rc_cart_write(c,(uint16_t)((which?0xe000:0x8000)+off),which?RC_CART_ROMH:RC_CART_ROML,v);}
static void erase(rc_cart *c,unsigned which,int chip){bank(c,0);command(c,which,0x80);unsigned select=which?RC_CART_ROMH:RC_CART_ROML;uint16_t base=which?0xe000:0x8000;rc_cart_write(c,base+0x555,select,0xaa);rc_cart_write(c,base+0x2aa,select,0x55);rc_cart_write(c,chip?base+0x555:base,select,chip?0x10:0x30);}
static int snapshot(const rc_cart *c,rc_state_writer *w){rc_sw_init(w);rc_sw_header(w,"easyflash","fixture",NULL,0);rc_sw_begin(w,"CART",0);rc_cart_save(c,w);rc_sw_end(w);return rc_sw_finish(w);}
int main(int argc,char **argv){
    if(argc==3&&!strcmp(argv[1],"--fixture")){rc_media *boot=fixture(argv[2]);CHECK(boot);boot->dirty=1;CHECK(rc_media_save(boot));rc_media_close(boot);puts("Original bootable EasyFlash writer fixture saved");return 0;}
    char path[2048],error[128];snprintf(path,sizeof path,"%s/easyflash-written.crt",argc>1?argv[1]:"work");rc_media *m=fixture(path);CHECK(m);rc_cart c;rc_cart_init(&c);rc_easyflash e;rc_easyflash_init(&e);CHECK(rc_cart_attach(&c,m,error,sizeof error)&&rc_easyflash_attach(&e,&c,1000000,error,sizeof error));
    rc_cart_lines l=rc_cart_get_lines(&c);CHECK(!l.game&&l.exrom);rc_cart_write(&c,0xde02,RC_CART_IO1,4);l=rc_cart_get_lines(&c);CHECK(l.game&&l.exrom);rc_cart_write(&c,0xdf21,RC_CART_IO2,0x43);CHECK(rc_cart_read(&c,0xdf21,RC_CART_IO2,0,1,0)==0x43&&rc_cart_drive_mask(&c,0xdf21,RC_CART_IO2,0)==255&&rc_cart_drive_mask(&c,0xde00,RC_CART_IO1,0)==0);rc_cart_write(&c,0xde02,RC_CART_IO1,5);
    /* Both devices identify themselves; register reads remain floating. */
    command(&c,0,0x90);CHECK(rc_cart_read(&c,0x8000,RC_CART_ROML,0,1,0)==1&&rc_cart_read(&c,0x8001,RC_CART_ROML,0,1,0)==0xa4);rc_cart_write(&c,0x8000,RC_CART_ROML,0xf0);
    command(&c,1,0x90);CHECK(rc_cart_read(&c,0xe001,RC_CART_ROMH,0,1,0)==0xa4);rc_cart_write(&c,0xe000,RC_CART_ROMH,0xf0);
    /* Start a write in a bank omitted by the sparse CRT. */
    program(&c,0,17,0x123,0x5a);uint8_t a=rc_cart_read(&c,0x8123,RC_CART_ROML,0,0,0);CHECK(a==rc_cart_read(&c,0x8123,RC_CART_ROML,0,0,0));uint8_t b=rc_cart_read(&c,0x8123,RC_CART_ROML,0,1,0);CHECK(((b^rc_cart_read(&c,0x8123,RC_CART_ROML,0,1,0))&0x40u)!=0);rc_cart_tick(&c,3,1);CHECK(e.flash[0].operation&&m->size==8272);
    rc_state_writer saved,expected,actual;CHECK(snapshot(&c,&saved));rc_cart_tick(&c,4,0);CHECK(!e.flash[0].operation&&m->dirty&&m->size==16480&&!e.sync_failed);CHECK(rc_cart_read(&c,0x8123,RC_CART_ROML,0,1,0)==0x5a);CHECK(snapshot(&c,&expected));
    /* Restore MID-busy after its completion appended a new bank packet. */
    rc_state_reader r;CHECK(rc_sr_open(&r,saved.data,saved.size,error,sizeof error)&&rc_sr_chunk(&r,"CART",0));
    CHECK(rc_cart_load(&c,&r));CHECK(c.crt.image==m&&e.flash[0].remaining==4);rc_cart_tick(&c,4,0);CHECK(snapshot(&c,&actual));CHECK(expected.size==actual.size&&!memcmp(expected.data,actual.data,actual.size));rc_sw_free(&saved);rc_sw_free(&expected);rc_sw_free(&actual);
    CHECK(rc_media_save(m));rc_media *disk=rc_media_open(path,0,error,sizeof error);CHECK(disk&&disk->size==m->size&&!memcmp(disk->data,m->data,m->size));rc_crt parsed;rc_crt_init(&parsed);CHECK(rc_crt_attach(&parsed,disk,error,sizeof error));const rc_crt_chip *ch=rc_crt_find(&parsed,17,0x8123);CHECK(ch&&ch->type==2&&disk->data[ch->offset+0x123]==0x5a);rc_crt_close(&parsed);rc_media_close(disk);
    /* Embedded flash ignores reset while busy; the cartridge reset changes
     * latches only. A literal F0 program byte is not a reset command. */
    program(&c,1,0,0x456,0xf0);rc_cart_reset(&c);CHECK(e.flash[1].operation);rc_cart_tick(&c,7,1);CHECK(e.flash[1].memory[0x456]==0xf0);
    program(&c,1,0,0x456,0xff);rc_cart_tick(&c,7,1);CHECK(e.flash[1].failed&&(rc_cart_read(&c,0xe456,RC_CART_ROMH,0,1,0)&0x20));rc_cart_write(&c,0xe000,RC_CART_ROMH,0xf0);CHECK(!e.flash[1].operation&&e.flash[1].memory[0x456]==0xf0);
    /* Multi-sector erase, suspend/program outside erase sector and resume. */
    erase(&c,0,0);bank(&c,16);rc_cart_write(&c,0x8000,RC_CART_ROML,0x30);CHECK(e.flash[0].sectors==5);rc_cart_tick(&c,50,1);CHECK(!e.flash[0].timeout);rc_cart_write(&c,0x8000,RC_CART_ROML,0xb0);CHECK(e.flash[0].suspended);uint8_t x=rc_cart_read(&c,0x8000,RC_CART_ROML,0,1,0),y=rc_cart_read(&c,0x8000,RC_CART_ROML,0,1,0);CHECK(((x^y)&4u)&&!((x^y)&0x40u));
    program(&c,0,40,7,0x30);rc_cart_tick(&c,7,1);CHECK(e.flash[0].suspended&&e.flash[0].memory[40*8192+7]==0x30);rc_cart_write(&c,0x8000,RC_CART_ROML,0x30);rc_cart_tick(&c,1000000,1);CHECK(!e.flash[0].operation&&e.flash[0].memory[17*8192+0x123]==255&&e.flash[0].memory[40*8192+7]==0x30);
    erase(&c,1,1);rc_cart_tick(&c,8000000,1);CHECK(!e.flash[1].operation&&e.flash[1].memory[0x456]==255);
    CHECK(rc_media_save(m));rc_easyflash_close(&e);rc_cart_close(&c);rc_media_close(m);CHECK(remove(path)==0);
    puts("EasyFlash banks/RAM, two flash command engines, sparse CRT persistence, status/suspend and exact MID-busy state passed");return 0;
}
