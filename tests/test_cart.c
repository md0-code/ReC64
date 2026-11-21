#include "../src/hw/cart.h"
#include "../src/core/font8x8.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define CHECK(x) do{if(!(x)){fprintf(stderr,"line %d: %s\n",__LINE__,#x);return 1;}}while(0)
typedef struct fixture {uint8_t *data;size_t size;} fixture;
static void put16(uint8_t *p,unsigned n){p[0]=(uint8_t)(n>>8);p[1]=(uint8_t)n;}
static void put32(uint8_t *p,unsigned n){p[0]=(uint8_t)(n>>24);p[1]=(uint8_t)(n>>16);p[2]=(uint8_t)(n>>8);p[3]=(uint8_t)n;}
static fixture header(unsigned type,int game,int exrom,const char *name){fixture f={calloc(1024*1024,1),64};if(!f.data)return f;memcpy(f.data,"C64 CARTRIDGE   ",16);put32(f.data+16,64);put16(f.data+20,0x100);put16(f.data+22,type);f.data[24]=(uint8_t)exrom;f.data[25]=(uint8_t)game;snprintf((char *)f.data+32,32,"%s",name);return f;}
static uint8_t *chip(fixture *f,unsigned bank,unsigned address,unsigned size,uint8_t value){uint8_t *p=f->data+f->size;memcpy(p,"CHIP",4);put32(p+4,size+16);put16(p+10,bank);put16(p+12,address);put16(p+14,size);memset(p+16,value,size);f->size+=size+16;return p+16;}
static int save_state(const rc_cart *c,rc_state_writer *w){rc_sw_init(w);rc_sw_header(w,"cart","fixture",NULL,0);rc_sw_begin(w,"CART",0);rc_cart_save(c,w);rc_sw_end(w);return rc_sw_finish(w);}
static int roundtrip(rc_cart *c){
    char error[128];rc_state_writer a,b;CHECK(save_state(c,&a));rc_media *borrowed=c->crt.image;uint16_t bank=c->bank;rc_cart_reset(c);
    rc_state_reader r;CHECK(rc_sr_open(&r,a.data,a.size,error,sizeof error)&&rc_sr_chunk(&r,"CART",0)&&rc_cart_load(c,&r));
    CHECK(c->bank==bank&&c->crt.image==borrowed&&!c->crt.owned);CHECK(save_state(c,&b));CHECK(a.size==b.size&&!memcmp(a.data,b.data,a.size));rc_sw_free(&a);rc_sw_free(&b);return 0;
}
/* Original startup code. All CPU stores use actual hardware registers. This
 * fixture is intentionally not a dump, crack or patched commercial program. */
static void firmware(uint8_t *rom,unsigned base,unsigned size,const char *name,int ultimax){
    memset(rom,0xff,size);unsigned entry=base+0x100;rom[0]=(uint8_t)entry;rom[1]=(uint8_t)(entry>>8);rom[2]=rom[0];rom[3]=rom[1];memcpy(rom+4,"\xc3\xc2\xcd\x38\x30",5);
    uint8_t code[]={
        0x78,0xd8,0xa2,0xff,0x9a,                 /* sei/cld/ldx/txs */
        0xa9,0x7f,0x8d,0x0d,0xdc,0x8d,0x0d,0xdd,
        0xa9,0x03,0x8d,0x02,0xdd,0x8d,0x00,0xdd,
        0xa9,0x00,0x8d,0x1a,0xd0,
        0xa9,0x1b,0x8d,0x11,0xd0,0xa9,0x08,0x8d,0x16,0xd0,
        0xa9,0x14,0x8d,0x18,0xd0,
        0xa9,0x06,0x8d,0x20,0xd0,0xa9,0x00,0x8d,0x21,0xd0,
        0xa2,0x00,0xa9,0x20,
        0x9d,0x00,0x04,0x9d,0x00,0x05,0x9d,0x00,0x06,0x9d,0x00,0x07,0xe8,0xd0,0xf1,
        0xa9,0x01,0x9d,0x00,0xd8,0x9d,0x00,0xd9,0x9d,0x00,0xda,0x9d,0x00,0xdb,0xe8,0xd0,0xf1,
        0xa2,0x00,0xbd,0x00,0x82,0xf0,0x06,0x9d,0x00,0x04,0xe8,0xd0,0xf5,
        0x4c,0x00,0x00
    };
    /* Relocate the two address operands in the original instruction stream. */
    for(size_t i=0;i+4<sizeof code;++i){if(code[i]==0xa9&&code[i+1]==0x14&&code[i+2]==0x8d&&code[i+3]==0x18&&code[i+4]==0xd0)code[i+1]=ultimax?0x1e:0x14;}
    code[sizeof code-12]=(uint8_t)((base+0x200)>>8);
    unsigned loop=entry+(unsigned)sizeof code-3;code[sizeof code-2]=(uint8_t)loop;code[sizeof code-1]=(uint8_t)(loop>>8);
    memcpy(rom+0x100,code,sizeof code);
    for(size_t i=0;i<strlen(name);++i){uint8_t c=(uint8_t)name[i];rom[0x200+i]=(c>='A'&&c<='Z')?(uint8_t)(c-64):c;}rom[0x200+strlen(name)]=0;
    if(ultimax){
        /* VIC Ultimax character reads at $3800 are ROMH $F800. Own glyphs. */
        for(unsigned c=0;c<256;++c){unsigned ascii=c<32?c+64:c;memset(rom+0x1800+c*8,0,8);if(ascii>=32&&ascii<128)memcpy(rom+0x1800+c*8,rc_font8x8_ascii[ascii-32],8);}
        for(unsigned off=0x1ffa;off<0x2000;off+=2){rom[off]=(uint8_t)entry;rom[off+1]=(uint8_t)(entry>>8);}
    }
}
static int write_fixture(const char *folder,const char *name,const fixture *f){char path[2048];snprintf(path,sizeof path,"%s/%s.crt",folder,name);FILE *out=fopen(path,"wb");if(!out)return 0;int ok=fwrite(f->data,1,f->size,out)==f->size;if(fclose(out))ok=0;return ok;}
static int emit(const char *folder){
    const unsigned ids[]={0,0,0,4,5,7,8,15,17,18,19};
    const char *names[]={"NORMAL 8K","NORMAL 16K","ULTIMAX","SIMONS BASIC","OCEAN","FUN PLAY","SUPER GAMES","SYSTEM 3","DINAMIC","ZAXXON","MAGIC DESK"};
    const char *files[]={"normal8k","normal16k","ultimax","simonsbasic","ocean","funplay","supergames","system3","dinamic","zaxxon","magicdesk"};
    for(unsigned i=0;i<11;++i){fixture f=header(ids[i],i==0||i==7||i==8||i==10,i==2,names[i]);CHECK(f.data);
        unsigned base=i==2?0xe000:0x8000,size=i==1||i==6?0x4000:i==9?0x1000:0x2000;
        uint8_t *rom=chip(&f,0,base,size,0xff);firmware(rom,base,size,names[i],i==2);
        if(i==3)chip(&f,0,0xa000,0x2000,0xff);
        if(i==9){chip(&f,0,0xa000,0x2000,0x11);chip(&f,1,0xa000,0x2000,0x22);}
        CHECK(write_fixture(folder,files[i],&f));free(f.data);
    }return 0;
}
int main(int argc,char **argv){
    char error[128];rc_cart c;rc_cart_init(&c);rc_cart_lines lines=rc_cart_get_lines(&c);CHECK(lines.game&&lines.exrom&&lines.nmi&&lines.irq&&lines.dma);
    fixture f=header(0,0,0,"NORMAL");CHECK(f.data);chip(&f,0,0x8000,0x4000,0x36);
    rc_media *m=rc_media_from("original-test.crt",f.data,f.size);CHECK(m&&rc_cart_attach(&c,m,error,sizeof error));CHECK(rc_cart_read(&c,0xbfff,RC_CART_ROMH,0,1,0)==0x36);CHECK(rc_cart_drive_mask(&c,0xbfff,RC_CART_ROMH,0)==255&&rc_cart_drive_mask(&c,0xde00,RC_CART_IO1,0)==0);CHECK(!roundtrip(&c));
    /* A truncated packet fails without detaching the cartridge. */
    m->data[68]=0xff;CHECK(!rc_cart_attach(&c,m,error,sizeof error)&&c.crt.image==m);m->data[68]=0;rc_cart_close(&c);rc_media_close(m);free(f.data);
    const unsigned ids[]={4,5,7,8,15,17,18,19};
    for(unsigned n=0;n<8;++n){unsigned id=ids[n];f=header(id,0,0,"MAPPER");CHECK(f.data);
        if(id==18){chip(&f,0,0x8000,0x1000,0x31);chip(&f,0,0xa000,0x2000,0x41);chip(&f,1,0xa000,0x2000,0x51);}
        else {chip(&f,0,0x8000,id==8?0x4000:0x2000,0x31);if(id==4)chip(&f,0,0xa000,0x2000,0x41);else chip(&f,id==7?8:1,0x8000,id==8?0x4000:0x2000,0x51);}
        m=rc_media_from("mapper-test.crt",f.data,f.size);CHECK(m&&rc_cart_attach(&c,m,error,sizeof error));
        if(id==4){CHECK(!rc_cart_get_lines(&c).game);rc_cart_read(&c,0xde00,RC_CART_IO1,0xa5,0,0);CHECK(!rc_cart_get_lines(&c).game);rc_cart_read(&c,0xde00,RC_CART_IO1,0xa5,1,0);CHECK(rc_cart_get_lines(&c).game);rc_cart_write(&c,0xde00,RC_CART_IO1,0);CHECK(!rc_cart_get_lines(&c).game);}
        else if(id==17){rc_cart_read(&c,0xde01,RC_CART_IO1,0xa5,0,0);CHECK(c.bank==0);rc_cart_read(&c,0xde01,RC_CART_IO1,0xa5,1,0);}
        else if(id==18){CHECK(rc_cart_read(&c,0x9000,RC_CART_ROML,0,1,0)==0x31);CHECK(rc_cart_read(&c,0xa000,RC_CART_ROMH,0,1,0)==0x51);}
        else rc_cart_write(&c,id==8?0xdf00:id==15?0xde01:0xde00,id==8?RC_CART_IO2:RC_CART_IO1,id==7?8:id==8?9:1);
        if(id!=4&&id!=18)CHECK(rc_cart_read(&c,0x8000,RC_CART_ROML,0,1,0)==0x51);
        if(id==8){rc_cart_write(&c,0xdf00,RC_CART_IO2,0);CHECK(c.bank==1&&c.locked);}
        CHECK(!roundtrip(&c));
        if(id==7||id==19){rc_cart_write(&c,0xde00,RC_CART_IO1,id==7?0x86:0x80);lines=rc_cart_get_lines(&c);CHECK(lines.game&&lines.exrom);}
        rc_cart_close(&c);rc_media_close(m);free(f.data);
    }
    f=header(0,0,1,"ULTIMAX");CHECK(f.data);chip(&f,0,0xe000,0x2000,0x63);m=rc_media_from("ultimax-test.crt",f.data,f.size);CHECK(m&&rc_cart_attach(&c,m,error,sizeof error));CHECK(rc_cart_read(&c,0xfffc,RC_CART_ROMH,0,1,1)==0x63);CHECK(!roundtrip(&c));rc_cart_close(&c);rc_media_close(m);free(f.data);
    f=header(RC_CRT_EPYX,1,1,"EPYX");CHECK(f.data);chip(&f,0,0x8000,0x2000,0x5a);
    m=rc_media_from("epyx-test.crt",f.data,f.size);CHECK(m&&rc_cart_attach(&c,m,error,sizeof error));
    CHECK(!rc_cart_get_lines(&c).exrom&&rc_cart_read(&c,0xdf40,RC_CART_IO2,0,1,0)==0x5a);
    rc_cart_tick(&c,511,1);CHECK(!roundtrip(&c)&&c.capacitor==1);rc_cart_tick(&c,1,1);CHECK(rc_cart_get_lines(&c).exrom);
    rc_cart_read(&c,0xde00,RC_CART_IO1,0xa5,0,0);CHECK(rc_cart_get_lines(&c).exrom);
    rc_cart_read(&c,0xde00,RC_CART_IO1,0xa5,1,0);CHECK(c.capacitor==512&&!rc_cart_get_lines(&c).exrom);
    rc_cart_close(&c);rc_media_close(m);free(f.data);
    /* Flash storage is changed in the borrowed file, including dirty state. */
    f=header(32,0,1,"FLASH STORAGE");CHECK(f.data);chip(&f,0,0x8000,0x2000,0xff);put16(f.data+72,2);m=rc_media_from("flash-test.crt",f.data,f.size);CHECK(m);m->writable=1;CHECK(rc_cart_attach(&c,m,error,sizeof error));CHECK(rc_crt_write(&c.crt,&c.crt.chips[0],3,0x5a)&&m->dirty&&m->data[83]==0x5a);CHECK(!roundtrip(&c));rc_cart_close(&c);rc_media_close(m);free(f.data);
    if(argc==3&&!strcmp(argv[1],"--fixtures"))CHECK(!emit(argv[2]));
    puts("CRT packets, game mapper registers/windows, floating reads, borrowed flash storage and exact states passed");return 0;
}
