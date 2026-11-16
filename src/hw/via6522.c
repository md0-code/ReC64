#include "via6522.h"
#include <string.h>

static void line(rc_via6522 *v,int pin,int level) {
    if(v->dev.host&&v->dev.host->line)rc_dev_line(&v->dev,pin,level);
}
static void irq(rc_via6522 *v) {
    const uint8_t next=(v->ifr&v->ier&127u)!=0;
    if(next!=v->irq){v->irq=next;line(v,RC_VIA_IRQ,next);}
}
static unsigned mode(const rc_via6522 *v,unsigned port){return v->pcr>>(port?5:1)&7u;}
static unsigned shift_mode(const rc_via6522 *v){return v->acr>>2&7u;}
static int internal_clock(unsigned m){return m&&m!=3&&m!=7;}
static int t2_clock(unsigned m){return m==1||m==4||m==5;}
uint8_t rc_via6522_port_output(const rc_via6522 *v,unsigned port) {
    if(port>1)return 0xff;
    uint8_t data=v->port[port];
    if(port==1&&(v->acr&128u))data=(uint8_t)((data&127u)|(v->t1_pb7<<7));
    return (uint8_t)(data|~v->ddr[port]);
}
int rc_via6522_control_output(const rc_via6522 *v,int pin) {
    const unsigned m=shift_mode(v);
    if(pin==RC_VIA_CA2)return mode(v,0)>=4?v->control[0]:-1;
    if(pin==RC_VIA_CB1)return internal_clock(m)?v->sr_clock:-1;
    if(pin==RC_VIA_CB2)return m>=4?v->sr_output:m?-1:mode(v,1)>=4?v->control[1]:-1;
    return -1;
}
static void outputs(rc_via6522 *v) {
    for(unsigned p=0;p<2;++p){const uint8_t data=rc_via6522_port_output(v,p);
        for(unsigned i=0;i<8;++i)line(v,(p?RC_VIA_PB0:RC_VIA_PA0)+(int)i,data>>i&1u);}
    line(v,RC_VIA_CA2_OUT,rc_via6522_control_output(v,RC_VIA_CA2));
    line(v,RC_VIA_CB1_OUT,rc_via6522_control_output(v,RC_VIA_CB1));
    line(v,RC_VIA_CB2_OUT,rc_via6522_control_output(v,RC_VIA_CB2));
}
static void acknowledge(rc_via6522 *v,unsigned port,int write) {
    const unsigned m=mode(v,port),flag=port?16u:2u;
    v->ifr&=(uint8_t)~(flag|((m==1||m==3)?0:flag>>1));
    if((!port||write)&&(m==4||m==5)){
        v->control[port]=0;v->pulse[port]=(uint8_t)(m==5);
    }
    irq(v);outputs(v);
}
static uint8_t port_read(const rc_via6522 *v,unsigned port) {
    const uint8_t pins=(v->acr&(1u<<port))?v->latched[port]:v->input[port];
    if(!port)return v->acr&1u?pins:pins&rc_via6522_port_output(v,0);
    const uint8_t out=rc_via6522_port_output(v,1);
    return (uint8_t)((pins&~v->ddr[1])|(out&v->ddr[1]));
}
static void sr_start(rc_via6522 *v) {
    const unsigned m=shift_mode(v);
    const int restart=(v->ifr&4u)||!v->sr_active;
    v->ifr&=0xfbu;v->sr_count=0;v->sr_pending=0;v->sr_active=(uint8_t)(m!=0);
    if(restart){v->sr_clock=1;if(t2_clock(m)){v->t2=(uint16_t)((v->t2&0xff00u)|v->t2_latch);v->t2_delay=1;v->t2_reload=0;}}
    irq(v);outputs(v);
}
/* CB2 changes on falling shift clocks; input and the modulo-8 counter
 * advance on rising clocks. External rising edges are sampled next phi2. */
static void sr_fall(rc_via6522 *v) {
    if(shift_mode(v)>=4&&v->sr_active){
        v->sr_output=v->sr>>7;v->sr=(uint8_t)((v->sr<<1)|v->sr_output);
    }
}
static void sr_rise(rc_via6522 *v) {
    const unsigned m=shift_mode(v);
    if(!m){v->sr=(uint8_t)((v->sr<<1)|v->pins[RC_VIA_CB2]);return;}
    if(!v->sr_active)return;
    if(m<4)v->sr=(uint8_t)((v->sr<<1)|v->pins[RC_VIA_CB2]);
    if(++v->sr_count==8){
        v->sr_count=0;
        if(m!=4)v->ifr|=4u;
        if(m==1||m==2||m==5||m==6)v->sr_active=0;
    }
}
void rc_via6522_tick(rc_via6522 *v) {
    ++v->cycles;
    for(unsigned p=0;p<2;++p)if(v->pulse[p]){v->pulse[p]=0;v->control[p]=1;}
    if(v->t1_delay)v->t1_delay=0;
    else if(v->t1_reload){v->t1_reload=0;v->t1=v->t1_latch;}
    else if(v->t1--==0){
        if(v->t1_armed||(v->acr&64u)){
            v->ifr|=64u;v->t1_pb7=(uint8_t)(v->acr&64u?v->t1_pb7^1u:1u);
        }
        v->t1_armed=0;if(v->acr&64u)v->t1_reload=1;
    }
    const unsigned m=shift_mode(v);int sr_edge=0;
    const uint8_t pb6=(uint8_t)((v->input[1]&rc_via6522_port_output(v,1))>>6&1u);
    if(v->t2_delay)v->t2_delay=0;
    else if(v->t2_reload){v->t2_reload=0;v->t2=(uint16_t)((v->t2&0xff00u)|v->t2_latch);}
    else if(!(v->acr&32u)||(v->pb6&&!pb6)){
        const uint16_t before=v->t2--;
        if(before==0&&v->t2_armed){v->ifr|=32u;v->t2_armed=0;}
        if(t2_clock(m)&&(before&255u)==0){v->t2_reload=1;sr_edge=1;}
    }
    v->pb6=pb6;
    if(v->sr_pending){v->sr_pending=0;sr_rise(v);}
    if(v->sr_active&&internal_clock(m)){
        if(m==2||m==6||sr_edge){v->sr_clock^=1;if(v->sr_clock)sr_rise(v);else sr_fall(v);}
    }
    irq(v);outputs(v);
}
uint8_t rc_via6522_read(rc_via6522 *v,unsigned reg) {
    reg&=15u;uint8_t value;
    switch(reg){
    case 0:case 1:value=port_read(v,reg^1u);acknowledge(v,reg^1u,0);return value;
    case 2:case 3:return v->ddr[reg^3u];
    case 4:value=(uint8_t)v->t1;v->ifr&=0xbfu;irq(v);return value;
    case 5:return (uint8_t)(v->t1>>8);
    case 6:return (uint8_t)v->t1_latch;
    case 7:return (uint8_t)(v->t1_latch>>8);
    case 8:value=(uint8_t)v->t2;v->ifr&=0xdfu;irq(v);return value;
    case 9:return (uint8_t)(v->t2>>8);
    case 10:value=v->sr;sr_start(v);return value;
    case 11:return v->acr;
    case 12:return v->pcr;
    case 13:return (uint8_t)(v->ifr|(v->irq?128u:0u));
    case 14:return v->ier|128u;
    default:return port_read(v,0); /* ORA without handshake or flag clear. */
    }
}
void rc_via6522_write(rc_via6522 *v,unsigned reg,uint8_t value) {
    reg&=15u;
    switch(reg){
    case 0:case 1:v->port[reg^1u]=value;acknowledge(v,reg^1u,1);return;
    case 2:case 3:v->ddr[reg^3u]=value;break;
    case 4:case 6:v->t1_latch=(uint16_t)((v->t1_latch&0xff00u)|value);break;
    case 5:v->t1_latch=(uint16_t)((v->t1_latch&255u)|value<<8);v->t1=v->t1_latch;
        v->t1_delay=v->t1_armed=1;v->t1_reload=0;v->t1_pb7=0;v->ifr&=0xbfu;break;
    case 7:v->t1_latch=(uint16_t)((v->t1_latch&255u)|value<<8);v->ifr&=0xbfu;break;
    case 8:v->t2_latch=value;break;
    case 9:v->t2=(uint16_t)(value<<8|v->t2_latch);v->t2_delay=(uint8_t)!(v->acr&32u);v->t2_armed=1;v->t2_reload=0;v->ifr&=0xdfu;break;
    case 10:v->sr=value;sr_start(v);return;
    case 11:{const unsigned old=shift_mode(v);v->acr=value;
        if(old!=shift_mode(v)){v->sr_active=v->sr_count=v->sr_pending=0;v->sr_clock=1;}
        if(!shift_mode(v))v->ifr&=0xfbu;break;}
    case 12:v->pcr=value;for(unsigned p=0;p<2;++p){const unsigned m=mode(v,p);v->pulse[p]=0;v->control[p]=(uint8_t)(m!=6);}break;
    case 13:v->ifr&=(uint8_t)~(value&127u);break;
    case 14:if(value&128u)v->ier|=value&127u;else v->ier&=(uint8_t)~value;break;
    case 15:v->port[0]=value;break;
    }
    irq(v);outputs(v);
}
void rc_via6522_input(rc_via6522 *v,int pin,int level) {
    if(pin<0||pin>3)return;level=level!=0;
    const int previous=v->pins[pin];v->pins[pin]=(uint8_t)level;
    if(previous==level)return;
    const unsigned p=(unsigned)pin/2,m=mode(v,p),sm=shift_mode(v);
    if(!(pin&1)){
        if(pin==RC_VIA_CB1&&!internal_clock(sm)){
            if(level)v->sr_pending=1;else sr_fall(v);
        }
        if(pin==RC_VIA_CB1&&internal_clock(sm))return;
        if((unsigned)level==((v->pcr>>(p?4:0))&1u)){
            v->ifr|=(uint8_t)(p?16:2);
            if(v->acr&(1u<<p))v->latched[p]=v->input[p]&rc_via6522_port_output(v,p);
            if(m==4)v->control[p]=1;
        }
    } else if(m<4&&!(p&&sm>=4)&&(unsigned)level==((m>>1)&1u))v->ifr|=(uint8_t)(p?8:1);
    irq(v);outputs(v);
}
void rc_via6522_port_input(rc_via6522 *v,unsigned port,uint8_t levels){if(port<2)v->input[port]=levels;}
void rc_via6522_reset(rc_via6522 *v) {
    const rc_dev dev=v->dev;const uint16_t t1=v->t1,latch=v->t1_latch,t2=v->t2;
    const uint8_t t2_latch=v->t2_latch,sr=v->sr;memset(v,0,sizeof *v);v->dev=dev;
    v->t1=t1;v->t1_latch=latch;v->t2=t2;v->t2_latch=t2_latch;v->sr=sr;v->t1_pb7=1;
    memset(v->input,255,2);memset(v->latched,255,2);memset(v->pins,1,4);
    v->control[0]=v->control[1]=v->sr_clock=v->sr_output=v->pb6=1;
    line(v,RC_VIA_IRQ,0);outputs(v);
}
void rc_via6522_save(const rc_via6522 *v,rc_state_writer *w) {
    rc_sw_u32(w,1);rc_sw_u64(w,v->cycles);rc_sw_u16(w,v->t1);rc_sw_u16(w,v->t1_latch);rc_sw_u16(w,v->t2);
    rc_sw_bytes(w,v->port,2);rc_sw_bytes(w,v->ddr,2);rc_sw_bytes(w,v->input,2);rc_sw_bytes(w,v->latched,2);
#define B(n) rc_sw_u8(w,v->n)
    B(acr);B(pcr);B(ifr);B(ier);B(irq);B(t1_delay);B(t1_reload);B(t1_armed);B(t1_pb7);
    B(t2_latch);B(t2_delay);B(t2_reload);B(t2_armed);B(pb6);
    rc_sw_bytes(w,v->pins,4);rc_sw_bytes(w,v->control,2);rc_sw_bytes(w,v->pulse,2);
    B(sr);B(sr_count);B(sr_active);B(sr_clock);B(sr_output);B(sr_pending);
#undef B
}
int rc_via6522_load(rc_via6522 *v,rc_state_reader *r) {
    if(r->end-r->at<54||rc_sr_u32(r)!=1)return 0;rc_via6522 next=*v;
    next.cycles=rc_sr_u64(r);next.t1=rc_sr_u16(r);next.t1_latch=rc_sr_u16(r);next.t2=rc_sr_u16(r);
    rc_sr_bytes(r,next.port,2);rc_sr_bytes(r,next.ddr,2);rc_sr_bytes(r,next.input,2);rc_sr_bytes(r,next.latched,2);
#define B(n) next.n=rc_sr_u8(r)
    B(acr);B(pcr);B(ifr);B(ier);B(irq);B(t1_delay);B(t1_reload);B(t1_armed);B(t1_pb7);
    B(t2_latch);B(t2_delay);B(t2_reload);B(t2_armed);B(pb6);
    rc_sr_bytes(r,next.pins,4);rc_sr_bytes(r,next.control,2);rc_sr_bytes(r,next.pulse,2);
    B(sr);B(sr_count);B(sr_active);B(sr_clock);B(sr_output);B(sr_pending);
#undef B
    if(!rc_sr_ok(r)||next.sr_count>7||next.irq!=((next.ifr&next.ier&127u)!=0))return 0;
    *v=next;outputs(v);line(v,RC_VIA_IRQ,v->irq);return 1;
}
static uint8_t dev_read(void *v,unsigned sp,uint32_t off){(void)sp;return rc_via6522_read(v,off);}
static void dev_write(void *v,unsigned sp,uint32_t off,uint8_t data){(void)sp;rc_via6522_write(v,off,data);}
static void dev_reset(void *v){rc_via6522_reset(v);}
static void dev_input(void *v,int pin,int level){rc_via6522_input(v,pin,level);}
static void dev_save(const void *v,rc_state_writer *w){rc_via6522_save(v,w);}
static int dev_load(void *v,rc_state_reader *r){return rc_via6522_load(v,r);}
static const rc_dev_ops OPS={.read=dev_read,.write=dev_write,.reset=dev_reset,.input=dev_input,.save=dev_save,.load=dev_load};
void rc_via6522_init(rc_via6522 *v,const rc_hw_host *host){memset(v,0,sizeof *v);v->dev=(rc_dev){&OPS,host,"VIA"};v->t1=v->t1_latch=v->t2=0xffff;v->t2_latch=255;rc_via6522_reset(v);}
