#include "am29f040.h"
#include <stdlib.h>
#include <string.h>
enum { IDLE, PROGRAM, SECTOR, CHIP };
enum { COMMAND, UNLOCK1, UNLOCK2, PROGRAM_DATA, ERASE_UNLOCK1, ERASE_UNLOCK2, ERASE_FINAL };
static uint64_t clocks(const rc_am29f040 *f,unsigned us){return ((uint64_t)f->clock_hz*us+999999u)/1000000u;}
void rc_am29f040_init(rc_am29f040 *f,uint8_t *memory,uint32_t hz){memset(f,0,sizeof *f);f->memory=memory;f->clock_hz=hz;}
static void array_mode(rc_am29f040 *f){
    f->command=COMMAND;f->autoselect=0;f->failed=0;f->toggle=0;
    if(f->suspended){f->operation=SECTOR;f->remaining=f->erase_remaining;}
    else {f->operation=IDLE;f->remaining=f->timeout=0;f->sectors=0;}
}
void rc_am29f040_reset(rc_am29f040 *f){
    if(f->operation==IDLE||f->failed||f->timeout||(f->operation==SECTOR&&f->suspended))array_mode(f);
}
static void changed(rc_am29f040 *f,uint32_t a,uint8_t v){if(f->memory[a]!=v){f->memory[a]=v;f->dirty_banks|=UINT64_C(1)<<(a>>13);}}
uint8_t rc_am29f040_read(rc_am29f040 *f,uint32_t a,int effects){
    a&=RC_AM29F040_SIZE-1u;
    if(f->autoselect){switch(a&0x43u){case 0:return 1;case 1:return 0xa4;case 2:return 0;default:return 0xff;}}
    if(f->operation==IDLE)return f->memory[a];
    const int selected=(f->sectors>>(a>>16))&1u;
    if(f->operation==SECTOR&&f->suspended&&!selected)return f->memory[a];
    uint8_t value=f->memory[a]&0x13u; /* DQ0, DQ1 and DQ4 are unspecified. */
    if(f->operation==PROGRAM)value|=(uint8_t)((~f->data)&0x80u);
    else if(f->suspended)value|=0x80u;
    if(f->failed)value|=0x20u;
    if(!f->suspended||f->operation==PROGRAM)value|=f->toggle&0x40u;
    if(f->operation!=PROGRAM&&selected)value|=f->toggle&4u;
    if(f->operation!=PROGRAM&&!f->timeout)value|=8u;
    if(effects){
        if(!f->suspended||f->operation==PROGRAM)f->toggle^=0x40u;
        if(f->operation!=PROGRAM&&selected)f->toggle^=4u;
    }return value;
}
void rc_am29f040_write(rc_am29f040 *f,uint32_t a,uint8_t v){
    a&=RC_AM29F040_SIZE-1u;unsigned command_address=a&0x7ffu;
    if(f->operation!=IDLE){
        if(f->failed){if(v==0xf0)array_mode(f);return;}
        if(f->operation==SECTOR){
            if(v==0xb0&&!f->suspended){f->timeout=0;f->suspended=1;f->erase_remaining=f->remaining;f->command=COMMAND;f->toggle&=4u;return;}
            if(f->suspended){if(v==0x30&&f->command==COMMAND&&!f->autoselect){f->suspended=0;return;}}
            else if(f->timeout){if(v==0x30){f->sectors|=(uint8_t)(1u<<(a>>16));f->timeout=clocks(f,50);return;}array_mode(f);return;}
            else return;
        }else return;
    }
    /* Program data is literal, including F0 (not a reset in this cycle). */
    if(f->command==PROGRAM_DATA){
        f->command=COMMAND;if(f->suspended&&((f->sectors>>(a>>16))&1u))return;
        f->address=a;f->data=v;f->operation=PROGRAM;f->remaining=clocks(f,7);f->toggle=0;f->autoselect=0;return;
    }
    if(v==0xf0){array_mode(f);return;}
    if(f->autoselect)return;
    switch(f->command){
    case COMMAND:f->command=(command_address==0x555&&v==0xaa)?UNLOCK1:COMMAND;break;
    case UNLOCK1:f->command=(command_address==0x2aa&&v==0x55)?UNLOCK2:COMMAND;break;
    case UNLOCK2:
        f->command=COMMAND;if(command_address!=0x555)break;
        if(v==0x90)f->autoselect=1;
        else if(v==0xa0)f->command=PROGRAM_DATA;
        else if(v==0x80&&!f->suspended)f->command=ERASE_UNLOCK1;
        break;
    case ERASE_UNLOCK1:f->command=(command_address==0x555&&v==0xaa)?ERASE_UNLOCK2:COMMAND;break;
    case ERASE_UNLOCK2:f->command=(command_address==0x2aa&&v==0x55)?ERASE_FINAL:COMMAND;break;
    case ERASE_FINAL:
        f->command=COMMAND;f->toggle=0;
        if(v==0x10&&command_address==0x555){f->operation=CHIP;f->sectors=255;f->remaining=clocks(f,8000000);}
        else if(v==0x30){f->operation=SECTOR;f->sectors=(uint8_t)(1u<<(a>>16));f->remaining=clocks(f,1000000);f->timeout=clocks(f,50);}
        break;
    default:f->command=COMMAND;break;
    }
}
void rc_am29f040_tick(rc_am29f040 *f,uint64_t cycles){
    if(f->operation==IDLE||f->failed||(f->suspended&&f->operation==SECTOR))return;
    if(f->timeout){if(cycles<f->timeout){f->timeout-=cycles;return;}cycles-=f->timeout;f->timeout=0;}
    if(cycles<f->remaining){f->remaining-=cycles;return;}f->remaining=0;
    if(f->operation==PROGRAM){
        uint8_t old=f->memory[f->address];changed(f,f->address,(uint8_t)(old&f->data));
        if((old&f->data)!=f->data){f->failed=1;return;}
    }else {
        for(unsigned sector=0;sector<8;++sector)if((f->sectors>>sector)&1u)
            for(uint32_t i=sector*0x10000u;i<(sector+1u)*0x10000u;++i)changed(f,i,255);
    }
    array_mode(f);
}
void rc_am29f040_save(const rc_am29f040 *f,rc_state_writer *w){
    rc_sw_u32(w,1);rc_sw_u32(w,f->clock_hz);rc_sw_u32(w,f->address);rc_sw_u64(w,f->remaining);rc_sw_u64(w,f->erase_remaining);rc_sw_u64(w,f->timeout);rc_sw_u64(w,f->dirty_banks);
    rc_sw_u8(w,f->command);rc_sw_u8(w,f->autoselect);rc_sw_u8(w,f->operation);rc_sw_u8(w,f->sectors);rc_sw_u8(w,f->suspended);rc_sw_u8(w,f->toggle);rc_sw_u8(w,f->data);rc_sw_u8(w,f->failed);rc_sw_packed(w,f->memory,RC_AM29F040_SIZE);
}
int rc_am29f040_load(rc_am29f040 *f,rc_state_reader *r){
    if(rc_sr_u32(r)!=1)return 0;rc_am29f040 n=*f;
    n.clock_hz=rc_sr_u32(r);n.address=rc_sr_u32(r);n.remaining=rc_sr_u64(r);n.erase_remaining=rc_sr_u64(r);n.timeout=rc_sr_u64(r);n.dirty_banks=rc_sr_u64(r);
    n.command=rc_sr_u8(r);n.autoselect=rc_sr_u8(r);n.operation=rc_sr_u8(r);n.sectors=rc_sr_u8(r);n.suspended=rc_sr_u8(r);n.toggle=rc_sr_u8(r);n.data=rc_sr_u8(r);n.failed=rc_sr_u8(r);
    if(!rc_sr_ok(r)||n.clock_hz!=f->clock_hz||n.address>=RC_AM29F040_SIZE||n.command>ERASE_FINAL||n.autoselect>1||n.operation>CHIP||n.suspended>1||n.failed>1||(n.toggle&~0x44u)||n.timeout>clocks(&n,50)||n.remaining>clocks(&n,8000000)||n.erase_remaining>clocks(&n,1000000)||
        (n.suspended&&(!n.sectors||n.operation==IDLE||n.operation==CHIP))||(n.timeout&&n.operation!=SECTOR))return 0;
    uint8_t *data=malloc(RC_AM29F040_SIZE);if(!data)return 0;rc_sr_packed(r,data,RC_AM29F040_SIZE);if(!rc_sr_ok(r)){free(data);return 0;}
    memcpy(f->memory,data,RC_AM29F040_SIZE);free(data);*f=n;return 1;
}
