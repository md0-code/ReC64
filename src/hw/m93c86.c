#include "m93c86.h"
#include <string.h>
enum {WAIT_START,COMMAND,DATA,READ,DONE,INVALID};
enum {NONE,WRITE,ERASE,WRITE_ALL,ERASE_ALL,ENABLE,DISABLE};
void rc_m93c86_init(rc_m93c86 *e,uint32_t hz){memset(e,0,sizeof *e);memset(e->memory,255,sizeof e->memory);e->clock_hz=hz;e->q=2;}
static void command(rc_m93c86 *e){
    const unsigned op=(e->shift>>10)&3u;e->address=(uint16_t)(e->shift&1023u);e->shift=0;e->bits=0;
    if(op==2){e->phase=READ;e->q=0;return;} /* READ begins with one dummy zero. */
    if(op==1){e->phase=DATA;e->pending=WRITE;return;}
    e->phase=DONE;
    if(op==3)e->pending=ERASE;
    else switch(e->address>>8){case 0:e->pending=DISABLE;break;case 1:e->pending=WRITE_ALL;e->phase=DATA;break;case 2:e->pending=ERASE_ALL;break;case 3:e->pending=ENABLE;break;}
}
void rc_m93c86_pins(rc_m93c86 *e,int cs,int clock,int data){
    cs=cs!=0;clock=clock!=0;data=data!=0;
    if(!cs){
        if(e->cs&&!e->remaining&&e->phase==DONE){
            if(e->pending==ENABLE)e->enabled=1;
            else if(e->pending==DISABLE)e->enabled=0;
            else if(e->enabled&&e->pending){e->operation=e->pending;e->remaining=((uint64_t)e->clock_hz*5u+999u)/1000u;e->ready=0;}
        }
        e->phase=WAIT_START;e->pending=NONE;e->bits=0;e->shift=0;e->q=2;
    }else if(!e->cs){e->q=e->remaining?0:e->ready?1:2;}
    if(cs&&!e->clock&&clock&&!e->remaining){
        switch(e->phase){
        case WAIT_START:if(data){e->shift=1;e->bits=1;e->phase=COMMAND;e->q=2;e->ready=0;}break;
        case COMMAND:e->shift=(e->shift<<1)|(unsigned)data;if(++e->bits==13)command(e);break;
        case DATA:e->shift=(e->shift<<1)|(unsigned)data;if(++e->bits==16){e->data=(uint16_t)e->shift;e->phase=DONE;}break;
        case READ:{const unsigned at=e->address*2u;e->q=(uint8_t)(((unsigned)e->memory[at+(e->bits>=8)]>>(7u-(e->bits&7u)))&1u);if(++e->bits==16){e->bits=0;e->address=(e->address+1u)&1023u;}break;}
        case DONE:e->phase=INVALID;e->pending=NONE;break; /* Extra clocks abort programming. */
        default:break;
        }
    }
    e->cs=(uint8_t)cs;e->clock=(uint8_t)clock;
}
void rc_m93c86_tick(rc_m93c86 *e,uint64_t cycles){
    if(!e->remaining)return;if(cycles<e->remaining){e->remaining-=cycles;return;}e->remaining=0;
    const unsigned first=e->operation==WRITE_ALL||e->operation==ERASE_ALL?0:e->address;
    const unsigned end=e->operation==WRITE_ALL||e->operation==ERASE_ALL?1024u:first+1u;
    const uint16_t value=e->operation==ERASE||e->operation==ERASE_ALL?65535u:e->data;
    for(unsigned a=first;a<end;++a){e->memory[a*2]=(uint8_t)(value>>8);e->memory[a*2+1]=(uint8_t)value;}
    e->operation=NONE;e->ready=1;e->q=e->cs?1:2;
}
void rc_m93c86_save(const rc_m93c86 *e,rc_state_writer *w){
    rc_sw_u32(w,1);rc_sw_u32(w,e->clock_hz);rc_sw_u32(w,e->shift);rc_sw_u64(w,e->remaining);rc_sw_u16(w,e->address);rc_sw_u16(w,e->data);
    rc_sw_u8(w,e->cs);rc_sw_u8(w,e->clock);rc_sw_u8(w,e->bits);rc_sw_u8(w,e->phase);rc_sw_u8(w,e->pending);rc_sw_u8(w,e->operation);rc_sw_u8(w,e->enabled);rc_sw_u8(w,e->q);rc_sw_u8(w,e->ready);rc_sw_bytes(w,e->memory,sizeof e->memory);
}
int rc_m93c86_load(rc_m93c86 *e,rc_state_reader *r){
    if(rc_sr_u32(r)!=1)return 0;rc_m93c86 n=*e;
    n.clock_hz=rc_sr_u32(r);n.shift=rc_sr_u32(r);n.remaining=rc_sr_u64(r);n.address=rc_sr_u16(r);n.data=rc_sr_u16(r);
    n.cs=rc_sr_u8(r);n.clock=rc_sr_u8(r);n.bits=rc_sr_u8(r);n.phase=rc_sr_u8(r);n.pending=rc_sr_u8(r);n.operation=rc_sr_u8(r);n.enabled=rc_sr_u8(r);n.q=rc_sr_u8(r);n.ready=rc_sr_u8(r);rc_sr_bytes(r,n.memory,sizeof n.memory);
    if(!rc_sr_ok(r)||n.clock_hz!=e->clock_hz||n.remaining>((uint64_t)n.clock_hz*5u+999u)/1000u||n.address>=1024||n.bits>16||n.phase>INVALID||n.pending>DISABLE||n.operation>ERASE_ALL||n.cs>1||n.clock>1||n.enabled>1||n.ready>1||n.q>2||((n.remaining!=0)!=(n.operation!=NONE)))return 0;
    *e=n;return 1;
}
