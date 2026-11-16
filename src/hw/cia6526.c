#include "cia6526.h"
#include <string.h>
static void line(rc_cia6526 *c,int pin,int value){if(c->dev.host&&c->dev.host->line)rc_dev_line(&c->dev,pin,value);}
static void set_irq(rc_cia6526 *c,int value){if(c->irq!=value){c->irq=(uint8_t)value;line(c,RC_CIA_IRQ,value);}}
static void request(rc_cia6526 *c,uint8_t bits){c->icr|=bits;if(c->icr&c->mask){if(c->model==RC_CIA_8521)set_irq(c,1);else c->irq_delay=1;}}
uint8_t rc_cia6526_port_output(const rc_cia6526 *c,unsigned port) {
    if(port>1)return 0xff;
    uint8_t v=(uint8_t)(c->port[port]|~c->ddr[port]);
    if(port==1)for(unsigned i=0;i<2;++i)if(c->timer[i].control&2u){
        const unsigned bit=64u<<i;const rc_cia_timer *t=&c->timer[i];
        v=(uint8_t)((v&~bit)|((t->control&4u?t->toggle:t->pulse)?bit:0));
    }
    return v;
}
static void outputs(rc_cia6526 *c){for(unsigned p=0;p<2;++p){const uint8_t v=rc_cia6526_port_output(c,p);for(unsigned i=0;i<8;++i)line(c,(p?RC_CIA_PB0:RC_CIA_PA0)+(int)i,(v>>i)&1u);}}
static int timer_tick(rc_cia_timer *t,int source) {
    t->pulse=0;
    t->count_pipe=(uint8_t)(((t->count_pipe<<1)|((t->control&1u)&&source))&7u);
    t->load_pipe=(uint8_t)(((t->load_pipe<<1)|t->force)&3u);t->force=0;
    if(t->count_pipe&4u)--t->counter;
    const int underflow=t->counter==0&&(t->count_pipe&2u);
    if(underflow){t->pulse=1;t->toggle^=1;if((t->control&8u)||t->oneshot){t->control&=0xfeu;t->count_pipe&=0xfeu;}}
    if(underflow||(t->load_pipe&2u)){t->counter=t->latch;t->count_pipe&=0xfdu;}
    t->oneshot=(uint8_t)(t->control&8u);return underflow;
}
static void serial_clock(rc_cia6526 *c) {
    if(!c->serial_active){if(!c->serial_pending)return;c->shift=c->sdr;c->serial_pending=0;c->serial_bits=0;c->serial_active=1;}
    c->cnt_out^=1;
    if(!c->cnt_out){c->sp_out=c->shift>>7;c->shift<<=1;line(c,RC_CIA_SP_OUT,c->sp_out);}
    else if(++c->serial_bits==8){c->serial_active=0;request(c,8);}
    line(c,RC_CIA_CNT_OUT,c->cnt_out);
}
void rc_cia6526_tick(rc_cia6526 *c) {
    ++c->cycles;if(c->irq_delay&&!--c->irq_delay){if(c->icr&c->mask)set_irq(c,1);}
    if(!c->pc){c->pc=1;line(c,RC_CIA_PC_OUT,1);}
    const int a=timer_tick(&c->timer[0],c->timer[0].control&32u?c->cnt_edge:1);
    const unsigned mode=c->timer[1].control>>5&3u;
    const int b=timer_tick(&c->timer[1],mode==0?1:mode==1?c->cnt_edge:mode==2?a:a&&c->cnt);
    if(a){request(c,1);if(c->timer[0].control&64u)serial_clock(c);}
    if(b)request(c,2);
    c->cnt_edge=0;outputs(c);
}
static void alarm(rc_cia6526 *c){const int match=!memcmp(c->tod,c->alarm,4);if(match&&!c->alarm_match)request(c,4);c->alarm_match=(uint8_t)match;}
static uint8_t bcd_increment(uint8_t v){return (uint8_t)((v&15u)==9?(v+7u):(v+1u));}
void rc_cia6526_tod_pulse(rc_cia6526 *c) {
    if(c->tod_stopped)return;
    if(++c->tod_divider<(c->timer[0].control&128u?5:6))return;
    c->tod_divider=0;
    if(++c->tod[0]==10){c->tod[0]=0;c->tod[1]=bcd_increment(c->tod[1]);
        if(c->tod[1]==0x60){c->tod[1]=0;c->tod[2]=bcd_increment(c->tod[2]);
            if(c->tod[2]==0x60){c->tod[2]=0;uint8_t h=c->tod[3]&31u,pm=c->tod[3]&128u;
                h=bcd_increment(h);if(h==0x12)pm^=128u;else if(h==0x13)h=1;c->tod[3]=h|pm;}}}
    alarm(c);
}
uint8_t rc_cia6526_read(rc_cia6526 *c,unsigned reg) {
    reg&=15u;
    if(reg<2){const uint8_t v=rc_cia6526_port_output(c,reg)&c->input[reg];if(reg==1){c->pc=0;line(c,RC_CIA_PC_OUT,0);}return v;}
    if(reg<4)return c->ddr[reg-2];
    if(reg<8){const rc_cia_timer *t=&c->timer[(reg-4)/2];return (uint8_t)(reg&1u?t->counter>>8:t->counter);}
    if(reg<12){if(reg==11&&!c->tod_latched){memcpy(c->tod_latch,c->tod,4);c->tod_latched=1;}uint8_t v=c->tod_latched?c->tod_latch[reg-8]:c->tod[reg-8];if(reg==8)c->tod_latched=0;return v;}
    if(reg==12)return c->sdr;
    if(reg==13){const uint8_t v=(uint8_t)(c->icr|(c->irq?128u:0u));c->icr=c->irq_delay=0;set_irq(c,0);return v;}
    return c->timer[reg-14].control&0xefu;
}
void rc_cia6526_write(rc_cia6526 *c,unsigned reg,uint8_t value) {
    reg&=15u;
    if(reg<2){c->port[reg]=value;if(reg==1){c->pc=0;line(c,RC_CIA_PC_OUT,0);}outputs(c);return;}
    if(reg<4){c->ddr[reg-2]=value;outputs(c);return;}
    if(reg<8){rc_cia_timer *t=&c->timer[(reg-4)/2];if(reg&1u){t->latch=(uint16_t)((t->latch&255u)|value<<8);if(!(t->control&1u)||t->pulse)t->counter=t->latch;}else {t->latch=(uint16_t)((t->latch&0xff00u)|value);if(t->pulse)t->counter=t->latch;}return;}
    if(reg<12){static const uint8_t MASK[4]={15,127,127,159};value&=MASK[reg-8];
        if(c->timer[1].control&128u)c->alarm[reg-8]=value;
        else {if(reg==11){c->tod_stopped=1;if((value&31u)==0x12)value^=128u;}c->tod[reg-8]=value;if(reg==8){c->tod_stopped=0;c->tod_divider=0;}}
        alarm(c);return;}
    if(reg==12){c->sdr=value;if(c->timer[0].control&64u)c->serial_pending=1;return;}
    if(reg==13){if(value&128u)c->mask|=value&31u;else c->mask&=(uint8_t)~(value&31u);
        if(c->icr&c->mask){if(c->model==RC_CIA_6526&&!c->irq)c->irq_delay=2;else request(c,0);}return;}
    rc_cia_timer *t=&c->timer[reg-14];const uint8_t old=t->control;
    if((value&1u)&&!(old&1u))t->toggle=1;
    if(value&16u)t->force=1;t->control=value&0xefu;
    if(reg==14&&((old^value)&64u)){c->serial_active=c->serial_pending=c->serial_bits=0;c->cnt_out=c->sp_out=1;line(c,RC_CIA_CNT_OUT,1);line(c,RC_CIA_SP_OUT,1);}
    outputs(c);
}
void rc_cia6526_input(rc_cia6526 *c,int pin,int level) {
    level=level!=0;
    if(pin==RC_CIA_CNT){if(level&&!c->cnt){c->cnt_edge=1;if(!(c->timer[0].control&64u)){c->shift=(uint8_t)((c->shift<<1)|c->sp);if(++c->serial_bits==8){c->sdr=c->shift;c->serial_bits=0;request(c,8);}}}c->cnt=(uint8_t)level;}
    else if(pin==RC_CIA_SP)c->sp=(uint8_t)level;
    else if(pin==RC_CIA_FLAG){if(!level&&c->flag)request(c,16);c->flag=(uint8_t)level;}
    else if(pin==RC_CIA_TOD){if(level&&!c->tod_pin)rc_cia6526_tod_pulse(c);c->tod_pin=(uint8_t)level;}
}
void rc_cia6526_port_input(rc_cia6526 *c,unsigned port,uint8_t pins){if(port<2)c->input[port]=pins;}
void rc_cia6526_reset(rc_cia6526 *c){const rc_dev dev=c->dev;const uint8_t model=c->model;memset(c,0,sizeof *c);c->dev=dev;c->model=model;c->input[0]=c->input[1]=0xff;c->cnt=c->sp=c->flag=c->pc=c->cnt_out=c->sp_out=1;c->tod[3]=1;for(unsigned i=0;i<2;++i)c->timer[i].counter=c->timer[i].latch=0xffff;line(c,RC_CIA_IRQ,0);outputs(c);}
void rc_cia6526_save(const rc_cia6526 *c,rc_state_writer *w) {
    rc_sw_u32(w,1);rc_sw_u64(w,c->cycles);
    for(unsigned i=0;i<2;++i){const rc_cia_timer *t=&c->timer[i];rc_sw_u16(w,t->counter);rc_sw_u16(w,t->latch);rc_sw_u8(w,t->control);rc_sw_u8(w,t->count_pipe);rc_sw_u8(w,t->load_pipe);rc_sw_u8(w,t->force);rc_sw_u8(w,t->oneshot);rc_sw_u8(w,t->toggle);rc_sw_u8(w,t->pulse);}
#define B(n) rc_sw_u8(w,c->n)
    B(model);rc_sw_bytes(w,c->port,2);rc_sw_bytes(w,c->ddr,2);rc_sw_bytes(w,c->input,2);
    B(icr);B(mask);B(irq);B(irq_delay);B(cnt);B(sp);B(flag);B(cnt_edge);B(tod_pin);B(pc);
    B(sdr);B(shift);B(serial_bits);B(serial_active);B(serial_pending);B(cnt_out);B(sp_out);
    rc_sw_bytes(w,c->tod,4);rc_sw_bytes(w,c->alarm,4);rc_sw_bytes(w,c->tod_latch,4);
    B(tod_latched);B(tod_stopped);B(tod_divider);B(alarm_match);
#undef B
}
int rc_cia6526_load(rc_cia6526 *c,rc_state_reader *r) {
    if(r->end-r->at<74||rc_sr_u32(r)!=1)return 0;c->cycles=rc_sr_u64(r);
    for(unsigned i=0;i<2;++i){rc_cia_timer *t=&c->timer[i];t->counter=rc_sr_u16(r);t->latch=rc_sr_u16(r);t->control=rc_sr_u8(r);t->count_pipe=rc_sr_u8(r);t->load_pipe=rc_sr_u8(r);t->force=rc_sr_u8(r);t->oneshot=rc_sr_u8(r);t->toggle=rc_sr_u8(r);t->pulse=rc_sr_u8(r);}
#define B(n) c->n=rc_sr_u8(r)
    B(model);rc_sr_bytes(r,c->port,2);rc_sr_bytes(r,c->ddr,2);rc_sr_bytes(r,c->input,2);
    B(icr);B(mask);B(irq);B(irq_delay);B(cnt);B(sp);B(flag);B(cnt_edge);B(tod_pin);B(pc);
    B(sdr);B(shift);B(serial_bits);B(serial_active);B(serial_pending);B(cnt_out);B(sp_out);
    rc_sr_bytes(r,c->tod,4);rc_sr_bytes(r,c->alarm,4);rc_sr_bytes(r,c->tod_latch,4);
    B(tod_latched);B(tod_stopped);B(tod_divider);B(alarm_match);
#undef B
    if(c->model>RC_CIA_8521||c->serial_bits>8||c->tod_divider>6)return 0;outputs(c);line(c,RC_CIA_IRQ,c->irq);return rc_sr_ok(r);
}
static uint8_t dev_read(void *d,unsigned sp,uint32_t off){(void)sp;return rc_cia6526_read(d,off);}
static void dev_write(void *d,unsigned sp,uint32_t off,uint8_t v){(void)sp;rc_cia6526_write(d,off,v);}
static void dev_reset(void *d){rc_cia6526_reset(d);}static void dev_input(void *d,int pin,int v){rc_cia6526_input(d,pin,v);}
static void dev_save(const void *d,rc_state_writer *w){rc_cia6526_save(d,w);}static int dev_load(void *d,rc_state_reader *r){return rc_cia6526_load(d,r);}
static const rc_dev_ops OPS={.read=dev_read,.write=dev_write,.reset=dev_reset,.input=dev_input,.save=dev_save,.load=dev_load};
void rc_cia6526_init(rc_cia6526 *c,const rc_hw_host *host,unsigned model){memset(c,0,sizeof *c);c->dev=(rc_dev){&OPS,host,"CIA"};c->model=(uint8_t)model;rc_cia6526_reset(c);}
