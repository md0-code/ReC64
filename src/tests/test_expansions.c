#include "reu.h"
#include "georam.h"
#include <stdio.h>
#include <string.h>
#define CHECK(x) do{if(!(x)){fprintf(stderr,"line %d: %s\n",__LINE__,#x);return 1;}}while(0)
static uint8_t memory[65536];
static void setup(rc_reu *r,uint16_t cpu,uint32_t ram,uint16_t count,uint8_t command){
    rc_reu_write(r,0xdf02,(uint8_t)cpu);rc_reu_write(r,0xdf03,(uint8_t)(cpu>>8));
    rc_reu_write(r,0xdf04,(uint8_t)ram);rc_reu_write(r,0xdf05,(uint8_t)(ram>>8));rc_reu_write(r,0xdf06,(uint8_t)(ram>>16));
    rc_reu_write(r,0xdf07,(uint8_t)count);rc_reu_write(r,0xdf08,(uint8_t)(count>>8));rc_reu_write(r,0xdf01,command);
}
static unsigned run(rc_reu *r){
    unsigned cycles=0;rc_reu_pins p;
    while((p=rc_reu_bus(r)).valid){uint8_t data=memory[p.address];if(p.write)memory[p.address]=p.data;rc_reu_tick(r,1,1,data);if(++cycles>131072)return 0;}
    return cycles;
}
static void save(const rc_reu *r,const rc_georam *g,rc_state_writer *w){
    rc_sw_init(w);rc_sw_header(w,"test","expansions",NULL,0);rc_sw_begin(w,"REU ",0);rc_reu_save(r,w);rc_sw_end(w);
    rc_sw_begin(w,"GEOR",0);rc_georam_save(g,w);rc_sw_end(w);rc_sw_finish(w);
}
int main(void){
    rc_reu r;rc_georam g;rc_reu_pins pins,stalled;rc_state_writer before,expected,actual;rc_state_reader reader;char error[80];
    CHECK(rc_reu_init(&r,0x1000000));CHECK(rc_georam_init(&g,0x400000));
    memory[0x2000]=0x35;memory[0x2001]=0x79;
    setup(&r,0x2000,0xfeffff,2,0x90);CHECK(run(&r)==2);CHECK(r.ram[0xfeffff]==0x35&&r.ram[0xff0000]==0x79);
    CHECK(rc_reu_read(&r,0xdf20,0,0)==0x50);CHECK(rc_reu_read(&r,0xdf07,0,0)==1);
    /* Debug status reads do not acknowledge; ordinary mirrored reads do. */
    CHECK(rc_reu_read(&r,0xdf00,0,1)==0x50);CHECK(rc_reu_read(&r,0xdf00,0,0)==0x10);
    rc_reu_write(&r,0xdf09,0xc0);setup(&r,0x3000,0xfeffff,2,0xb1);CHECK(run(&r)==2);
    CHECK(memory[0x3000]==0x35&&memory[0x3001]==0x79&&rc_reu_irq(&r));
    CHECK(rc_reu_read(&r,0xdf02,0,0)==0&&rc_reu_read(&r,0xdf03,0,0)==0x30);
    CHECK(rc_reu_read(&r,0xdf07,0,0)==2);(void)rc_reu_read(&r,0xdf00,0,1);CHECK(!rc_reu_irq(&r));
    /* Deferred trigger works without a register access and can be cancelled. */
    setup(&r,0x3000,0xfeffff,2,0x83);CHECK(!rc_reu_bus(&r).dma);rc_reu_observe_write(&r,0xff01);CHECK(!rc_reu_bus(&r).dma);
    rc_reu_observe_write(&r,0xff00);CHECK(run(&r)==2);CHECK(!(r.reg[0]&0x20));
    memory[0x3000]^=1;setup(&r,0x3000,0xfeffff,2,0x93);CHECK(run(&r)==1);CHECK((r.reg[0]&0x60)==0x60);
    CHECK(r.reg[7]==1);(void)rc_reu_read(&r,0xdf00,0,1);
    /* Half-autoload restores the paired address byte from its shadow. */
    setup(&r,0x20ff,0,1,0x90);CHECK(run(&r)==1&&r.reg[3]==0x21);rc_reu_write(&r,0xdf02,0x42);CHECK(r.reg[3]==0x20);
    /* Hold both addresses, repeating one location for the programmed length. */
    rc_reu_write(&r,0xdf0a,0xc0);setup(&r,0x2000,5,3,0x90);CHECK(run(&r)==3&&r.ram[5]==0x35&&r.reg[2]==0&&r.reg[4]==5);
    rc_reu_write(&r,0xdf0a,0);setup(&r,0x4000,5,1,0x91);CHECK(run(&r)==1&&memory[0x4000]==0x35);
    rc_georam_write(&g,0xdfff,255);rc_georam_write(&g,0xdffe,255);rc_georam_write(&g,0xde55,0xa6);
    CHECK(g.ram[0x3fff55]==0xa6&&rc_georam_read(&g,0xdffe,0x71,1)==0x71&&!rc_georam_drive_mask(&g,0xdfff));
    rc_georam_write(&g,0xdf80,0);CHECK(rc_georam_read(&g,0xde55,0,1)==0);rc_georam_write(&g,0xdffe,63);
    rc_georam_reset(&g);CHECK(rc_georam_read(&g,0xde55,0,0)==0xa6);
    /* Save between a swap's two accesses, after a VIC stall. */
    memory[0x2000]=0xe1;r.ram[9]=0x42;setup(&r,0x2000,9,1,0x92);pins=rc_reu_bus(&r);
    rc_reu_tick(&r,0,0,0);stalled=rc_reu_bus(&r);CHECK(!memcmp(&pins,&stalled,sizeof pins));
    rc_reu_tick(&r,1,0,0);CHECK(r.phase==0);rc_reu_tick(&r,1,1,memory[0x2000]);CHECK(r.phase==1);
    CHECK(rc_reu_read(&r,0xdf00,0x87,1)==0x87&&!rc_reu_drive_mask(&r,0xdf00));
    save(&r,&g,&before);CHECK(!before.failed);CHECK(run(&r)==1&&memory[0x2000]==0x42&&r.ram[9]==0xe1);save(&r,&g,&expected);
    CHECK(rc_sr_open(&reader,before.data,before.size,error,sizeof error));CHECK(rc_sr_chunk(&reader,"REU ",0)&&rc_reu_load(&r,&reader));
    CHECK(rc_sr_chunk(&reader,"GEOR",0)&&rc_georam_load(&g,&reader));CHECK(run(&r)==1);save(&r,&g,&actual);
    CHECK(!actual.failed&&actual.size==expected.size&&!memcmp(actual.data,expected.data,actual.size));
    rc_sw_free(&before);rc_sw_free(&expected);rc_sw_free(&actual);rc_reu_free(&r);rc_georam_free(&g);
    CHECK(rc_reu_init(&r,0x20000));setup(&r,0x2000,0x1ffff,2,0x90);CHECK(run(&r)==2&&r.ram[0]==0x79&&r.reg[6]==0);rc_reu_free(&r);
    CHECK(rc_reu_init(&r,0x40000));r.ram[0]=0x77;setup(&r,0x2000,0x40000,1,0x90);CHECK(run(&r)==1&&r.ram[0]==0x77);
    CHECK(rc_reu_read(&r,0xdf06,0,0)==0xfc&&rc_reu_read(&r,0xdf0b,0,0)==0xff);rc_reu_free(&r);
    puts("REU DMA/BA, commands, status, shadow registers and GeoRAM mapping with exact mid-swap state OK");return 0;
}
