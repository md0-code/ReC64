#include "reu.h"
#include <stdlib.h>
#include <string.h>

static int capacity(uint32_t n){return !n||(n>=0x20000u&&n<=0x1000000u&&!(n&(n-1)));}
static uint8_t bank_mask(const rc_reu *r){return r->size<=0x80000u?7:255;}
static uint16_t pair(const uint8_t *p){return (uint16_t)(p[0]|(uint16_t)p[1]<<8);}
static void set_pair(uint8_t *p,uint16_t v){p[0]=(uint8_t)v;p[1]=(uint8_t)(v>>8);}
static uint32_t ram_address(const rc_reu *r){return (uint32_t)pair(r->reg+4)|((uint32_t)r->reg[6]<<16);}
static uint8_t ram_read(const rc_reu *r){
    uint32_t a=ram_address(r);
    if(r->size==0x40000u&&a>=r->size)return r->data_latch;
    return r->ram[a&(r->size-1)];
}
static void ram_write(rc_reu *r,uint8_t data){
    uint32_t a=ram_address(r);r->data_latch=data;
    if(r->size==0x40000u&&a>=r->size)return;
    r->ram[a&(r->size-1)]=data;
}
static void interrupt(rc_reu *r){
    if((r->reg[9]&0x80)&&(r->reg[9]&r->reg[0]&0x60))r->reg[0]|=0x80;
}
int rc_reu_init(rc_reu *r,uint32_t bytes){
    memset(r,0,sizeof *r);if(!capacity(bytes))return 0;
    if(bytes){r->ram=(uint8_t*)calloc(bytes,1);if(!r->ram)return 0;}
    r->size=bytes;rc_reu_reset(r);return 1;
}
void rc_reu_free(rc_reu *r){free(r->ram);memset(r,0,sizeof *r);}
void rc_reu_reset(rc_reu *r){
    memset(r->reg,0,sizeof r->reg);r->reg[0]=r->size>0x20000u?0x10:0;
    r->reg[1]=0x10;r->reg[7]=r->reg[8]=0xff;r->reg[9]=0x1f;r->reg[10]=0x3f;
    memcpy(r->shadow,r->reg+2,7);r->active=r->phase=r->swap_byte=0;r->data_latch=0;r->cycles=0;
}
uint8_t rc_reu_drive_mask(const rc_reu *r,uint16_t a){return r->size&&!r->active&&(a&0xff00u)==0xdf00u?0xff:0;}
uint8_t rc_reu_read(rc_reu *r,uint16_t a,uint8_t bus,int effects){
    unsigned i=a&31u;uint8_t v;
    if(!rc_reu_drive_mask(r,a))return bus;
    if(i>=11)return 0xff;
    v=r->reg[i];
    if(i==6)v|=(uint8_t)~bank_mask(r);
    if(i==0&&effects)r->reg[0]&=0x1f;
    return v;
}
void rc_reu_write(rc_reu *r,uint16_t a,uint8_t v){
    unsigned i=a&31u;
    if(!rc_reu_drive_mask(r,a)||i==0||i>=11)return;
    if(i>=2&&i<=8){
        r->shadow[i-2]=i==6?(uint8_t)(v&bank_mask(r)):v;
        if(i==6)r->reg[6]=r->shadow[4];
        else {unsigned first=i<=3?2:i<=5?4:7;r->reg[first]=r->shadow[first-2];r->reg[first+1]=r->shadow[first-1];}
        return;
    }
    if(i==9)v|=0x1f;
    if(i==10)v|=0x3f;
    r->reg[i]=v;
    if(i==9)interrupt(r);
    if(i==1&&(v&0x90)==0x90)r->active=1;
}
void rc_reu_observe_write(rc_reu *r,uint16_t a){
    if(r->size&&!r->active&&a==0xff00&&(r->reg[1]&0x90)==0x80)r->active=1;
}
rc_reu_pins rc_reu_bus(const rc_reu *r){
    rc_reu_pins p={0};
    if(!r->active)return p;
    p.address=pair(r->reg+2);p.dma=p.valid=1;
    p.write=(uint8_t)((r->reg[1]&3)==1||((r->reg[1]&3)==2&&r->phase));
    if(p.write)p.data=ram_read(r);
    return p;
}
static void advance(rc_reu *r){
    if(!(r->reg[10]&0x80))set_pair(r->reg+2,(uint16_t)(pair(r->reg+2)+1));
    if(!(r->reg[10]&0x40)){
        uint32_t a=ram_address(r)+1;
        if(r->size==0x20000u&&a==0x20000u)a=0;
        a&=r->size<=0x80000u?0x7ffffu:0xffffffu;
        set_pair(r->reg+4,(uint16_t)a);r->reg[6]=(uint8_t)(a>>16);
    }
}
static void finish(rc_reu *r){
    r->active=r->phase=0;r->reg[1]=(uint8_t)((r->reg[1]&0x7f)|0x10);
    if(r->reg[1]&0x20)memcpy(r->reg+2,r->shadow,7);
    interrupt(r);
}
void rc_reu_tick(rc_reu *r,int ba,int accepted,uint8_t data){
    unsigned mode;uint16_t length;int mismatch=0;
    ++r->cycles;if(!r->active||!ba||!accepted)return;
    mode=r->reg[1]&3u;
    if(mode==2&&!r->phase){r->swap_byte=data;r->phase=1;return;}
    if(mode==0)ram_write(r,data);
    else if(mode==1)r->data_latch=ram_read(r);
    else if(mode==2){ram_write(r,r->swap_byte);r->phase=0;}
    else {uint8_t value=ram_read(r);r->data_latch=value;mismatch=data!=value;if(mismatch)r->reg[0]|=0x20;}
    advance(r);length=pair(r->reg+7);
    if(length==1){r->reg[0]|=0x40;finish(r);return;}
    --length;set_pair(r->reg+7,length);
    if(mismatch){if(length==1)r->reg[0]|=0x40;finish(r);}
}
int rc_reu_irq(const rc_reu *r){return r->size&&(r->reg[0]&0x80)!=0;}
void rc_reu_save(const rc_reu *r,rc_state_writer *w){
    rc_sw_u32(w,1);rc_sw_u32(w,r->size);rc_sw_u64(w,r->cycles);
    rc_sw_bytes(w,r->reg,11);rc_sw_bytes(w,r->shadow,7);
    rc_sw_u8(w,r->active);rc_sw_u8(w,r->phase);rc_sw_u8(w,r->swap_byte);rc_sw_u8(w,r->data_latch);
    rc_sw_packed(w,r->ram,r->size);
}
int rc_reu_load(rc_reu *r,rc_state_reader *s){
    rc_reu n;uint32_t size;
    if(rc_sr_u32(s)!=1)return 0;size=rc_sr_u32(s);
    if(!capacity(size)||!rc_reu_init(&n,size))return 0;
    n.cycles=rc_sr_u64(s);rc_sr_bytes(s,n.reg,11);rc_sr_bytes(s,n.shadow,7);
    n.active=rc_sr_u8(s);n.phase=rc_sr_u8(s);n.swap_byte=rc_sr_u8(s);n.data_latch=rc_sr_u8(s);
    rc_sr_packed(s,n.ram,n.size);
    if(!rc_sr_ok(s)||n.active>1||n.phase>1||(n.active&&(!n.size||!(n.reg[1]&0x80)))||
       (n.phase&&(!n.active||(n.reg[1]&3)!=2))||(n.reg[0]&0x1f)!=(n.size>0x20000u?0x10:0)||
       (n.reg[6]&~bank_mask(&n))||(n.shadow[4]&~bank_mask(&n))||
       (n.reg[9]&0x1f)!=0x1f||(n.reg[10]&0x3f)!=0x3f){rc_reu_free(&n);return 0;}
    rc_reu_free(r);*r=n;return 1;
}
