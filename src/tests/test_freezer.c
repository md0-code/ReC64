#include "cart_freezer.h"
#include <stdio.h>
#include <string.h>
#define CHECK(x) do{if(!(x)){fprintf(stderr,"line %d: %s\n",__LINE__,#x);return 1;}}while(0)
static uint8_t data[65536];
static rc_freezer f;
static const rc_cart_ops *o=&rc_freezer_ops;
static uint8_t rd(uint16_t a,unsigned s){return o->read(&f,a,s,0xd6,1,0);}
static void wr(uint16_t a,unsigned s,uint8_t v){o->write(&f,a,s,v);}
static void pulse(void){o->freeze(&f,1);o->tick(&f,3,1);o->freeze(&f,0);}
static int save(rc_state_writer *w){rc_sw_init(w);rc_sw_header(w,"test","freezer",NULL,0);rc_sw_begin(w,"FRZR",0);rc_freezer_save(&f,w);rc_sw_end(w);return rc_sw_finish(w);}
static int roundtrip(void){
    rc_state_writer a,b;rc_state_reader r;char err[128];CHECK(save(&a));
    o->reset(&f);memset(f.ram,0x9a,sizeof f.ram);
    CHECK(rc_sr_open(&r,a.data,a.size,err,sizeof err)&&rc_sr_chunk(&r,"FRZR",0)&&rc_freezer_load(&f,&r));
    CHECK(save(&b)&&a.size==b.size&&!memcmp(a.data,b.data,a.size));rc_sw_free(&a);rc_sw_free(&b);return 0;
}
int main(void){
    for(unsigned p=0;p<256;++p)memset(data+p*256,(int)p,256);
    rc_media media={.data=data,.size=sizeof data};rc_crt_chip chips[8];
    for(unsigned b=0;b<8;++b)chips[b]=(rc_crt_chip){0,(uint16_t)b,0x8000,8192,b*8192};
    rc_crt crt={.image=&media,.chips=chips,.count=4,.hardware=RC_CRT_ACTION_REPLAY};
    CHECK(rc_freezer_init(&f,&crt));CHECK(rd(0x8000,RC_CART_ROML)==0);
    wr(0xde00,RC_CART_IO1,0x18);CHECK(rd(0x8000,RC_CART_ROML)==0x60);
    wr(0xde00,RC_CART_IO1,0x20);wr(0x8000,0,0x51);CHECK(rd(0x8000,RC_CART_ROML)==0x51);
    wr(0xdf40,RC_CART_IO2,0x7b);CHECK(rd(0x9f40,RC_CART_ROML)==0x7b);
    o->read(&f,0xde00,RC_CART_IO1,0x18,0,0);CHECK(f.control==0x20);
    o->read(&f,0xde00,RC_CART_IO1,0x18,1,0);CHECK(f.control==0x18);
    wr(0xde00,RC_CART_IO1,4);CHECK(!o->drive_mask(&f,0xdf00,RC_CART_IO2,0));
    pulse();CHECK(!o->lines(&f).nmi&&!o->lines(&f).irq);
    o->tick(&f,4,1);o->tick(&f,1,0);CHECK(f.count==0);o->tick(&f,4,1);CHECK(roundtrip()==0);
    o->tick(&f,1,1);CHECK(f.frozen&&o->lines(&f).nmi&&!o->lines(&f).game&&o->lines(&f).exrom);
    CHECK(rd(0xfffa,RC_CART_ROMH)==0x1f&&!o->drive_mask(&f,0x8000,RC_CART_ROML,0));
    wr(0xde00,RC_CART_IO1,0x40);CHECK(!f.frozen&&f.ram[0]==0x51);

    crt.hardware=RC_CRT_ACTION_REPLAY4;CHECK(rc_freezer_init(&f,&crt));
    CHECK(o->lines(&f).game&&o->lines(&f).exrom);pulse();o->tick(&f,5,1);
    CHECK(f.enabled&&!o->lines(&f).game&&o->lines(&f).exrom&&rd(0xe000,RC_CART_ROMH)==0);
    wr(0xde00,RC_CART_IO1,0x1b);CHECK(rd(0xdf00,RC_CART_IO2)==0x7f);
    wr(0xde00,RC_CART_IO1,0x18);CHECK(!o->drive_mask(&f,0x8000,RC_CART_ROML,0));
    CHECK(rd(0xa000,RC_CART_ROMH)==0x40&&!o->drive_mask(&f,0xdf00,RC_CART_IO2,0));
    CHECK(roundtrip()==0);wr(0xde00,RC_CART_IO1,4);CHECK(!f.enabled);pulse();CHECK(f.pending);

    crt.hardware=RC_CRT_RETRO_REPLAY;crt.count=8;CHECK(rc_freezer_init(&f,&crt));
    wr(0xde01,RC_CART_IO1,0x42);wr(0xde01,RC_CART_IO1,0x04);CHECK(f.extended==0x42);
    wr(0xde00,RC_CART_IO1,0x38);wr(0xde02,RC_CART_IO1,0x92);
    CHECK(rd(0x9e02,RC_CART_ROML)==0x92&&!o->drive_mask(&f,0xdf00,RC_CART_IO2,0));
    pulse();o->observe(&f,0x1ff,1,1,0);o->observe(&f,0x1234,0,1,1);o->observe(&f,0x1fe,1,1,0);
    CHECK(roundtrip()==0);o->observe(&f,0x1fd,1,1,0);CHECK(f.frozen&&!f.pending);
    CHECK(rd(0xfffa,RC_CART_ROMH)==0x1f);wr(0xde00,RC_CART_IO1,0x58);CHECK(!f.frozen);
    CHECK(rd(0x8000,RC_CART_ROML)==0x60);
    o->reset(&f);wr(0xde01,RC_CART_IO1,4);pulse();CHECK(!f.pending);
    o->freeze(&f,1);CHECK(rd(0xde00,RC_CART_IO1)&4);o->freeze(&f,0);

    for(unsigned b=0;b<4;++b)chips[b]=(rc_crt_chip){0,(uint16_t)b,0x8000,16384,b*16384};
    crt.hardware=RC_CRT_FINAL_III;crt.count=4;CHECK(rc_freezer_init(&f,&crt));
    CHECK(!o->lines(&f).nmi&&!o->lines(&f).game&&!o->lines(&f).exrom);
    wr(0xdfff,RC_CART_IO2,0x73);CHECK(rd(0xde00,RC_CART_IO1)==0xde&&rd(0xdfff,RC_CART_IO2)==0xdf);
    CHECK(o->read(&f,0xfffa,RC_CART_ROMH,0,1,1)==0xff);
    wr(0xdfff,RC_CART_IO2,0xf3);wr(0xdfff,RC_CART_IO2,0);CHECK(f.control==0xf3);
    CHECK(o->drive_mask(&f,0xde00,RC_CART_IO1,0)==255);o->freeze(&f,1);o->tick(&f,6,1);
    CHECK(o->lines(&f).game&&!o->lines(&f).nmi);o->tick(&f,9,0);CHECK(f.count==6);
    CHECK(roundtrip()==0);o->tick(&f,1,1);CHECK(!o->lines(&f).game);
    wr(0xdfff,RC_CART_IO2,0x13);o->freeze(&f,0);CHECK(!o->lines(&f).game&&!o->lines(&f).nmi);
    wr(0xdfff,RC_CART_IO2,0x73);CHECK(o->lines(&f).nmi&&o->lines(&f).game);
    puts("Freezer banking, RAM, I/O, physical freeze sequencing and state OK");return 0;
}
