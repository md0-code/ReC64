/* 901227-03 locally disassembled in Ghidra: F49E/F4A5 LOAD,
 * F5DD/F5ED SAVE, F5A9/F68D exits and the F6FB-F713 error table. */
#include "traps.h"
#include "../core/romdb.h"
#include <string.h>
static void watches(rc_traps *t){
    memset(t->watch,0,sizeof t->watch);
    if(t->known&&t->load_enabled)t->watch[0xf4a5u>>3]|=(uint8_t)(1u<<(0xf4a5u&7u));
    if(t->known&&t->save_enabled)t->watch[0xf5edu>>3]|=(uint8_t)(1u<<(0xf5edu&7u));
}
void rc_traps_init(rc_traps *t,const uint8_t hash[32]){
    memset(t,0,sizeof *t);const rc_known_rom *rom=rc_romdb_find("kernal");
    if(!hash||!rom)return;
    static const char hex[]="0123456789abcdef";char text[65];
    for(unsigned i=0;i<32;++i){text[i*2]=hex[hash[i]>>4];text[i*2+1]=hex[hash[i]&15u];}text[64]=0;
    t->known=(uint8_t)!strcmp(text,rom->sha256);rc_traps_enable(t,1,1);
}
void rc_traps_enable(rc_traps *t,int load,int save){t->load_enabled=(uint8_t)(load!=0);t->save_enabled=(uint8_t)(save!=0);watches(t);}
static uint16_t read16(const rc_traps_io *io,uint16_t at){return (uint16_t)(io->read(io->ctx,at)|(uint16_t)io->read(io->ctx,(uint16_t)(at+1))<<8);}
static void write16(const rc_traps_io *io,uint16_t at,uint16_t value){io->write(io->ctx,at,(uint8_t)value);io->write(io->ctx,(uint16_t)(at+1),(uint8_t)(value>>8));}
static void filename(const rc_traps_io *io,uint8_t name[16],unsigned *length){
    const uint16_t at=read16(io,0xbb);*length=io->read(io->ctx,0xb7);if(*length>16)*length=16;
    memset(name,0,16);for(unsigned i=0;i<*length;++i)name[i]=io->read(io->ctx,(uint16_t)(at+i));
}
static uint16_t failure(int result){unsigned error=(unsigned)-result;if(error<1||error>9)error=5;return (uint16_t)(0xf6f8u+3u*error);}
int rc_traps_fetch(rc_traps *t,const rc_traps_io *io,uint16_t pc,uint8_t a,uint16_t *next_pc){
    if(!t->known||!io||!io->read||!io->write||!next_pc)return 0;
    if(pc==0xf4a5&&t->load_enabled&&io->load){
        uint8_t name[16];unsigned length;filename(io,name,&length);
        const unsigned device=io->read(io->ctx,0xba),secondary=io->read(io->ctx,0xb9);
        uint16_t address=0;const uint8_t *data=NULL;size_t size=0;
        const int result=io->load(io->ctx,device,secondary,name,length,&address,&data,&size);
        if(!result)return 0;
        io->write(io->ctx,0x93,a);io->write(io->ctx,0x90,0);
        if(result<0){*next_pc=failure(result);return 1;}
        if((size&&!data)||size>65536){*next_pc=failure(-5);return 1;}
        const uint16_t target=secondary||result==2?address:read16(io,0xc3);
        if(device==1)write16(io,0xc1,target);
        uint8_t status=0;
        for(size_t i=0;i<size;++i){const uint16_t at=(uint16_t)(target+i);
            if(!a)io->write(io->ctx,at,data[i]);
            else if(io->read(io->ctx,at)!=data[i]){
                status|=0x10;
                /* The serial VERIFY path F516-F51C stores its mismatch
                 * marker, unlike the tape comparison routine. */
                if(device>=4)io->write(io->ctx,at,0x10);
            }
        }
        io->write(io->ctx,0x90,status);write16(io,0xae,(uint16_t)(target+size));
        ++t->loads;*next_pc=0xf5a9;return 1;
    }
    if(pc==0xf5ed&&t->save_enabled&&io->save){
        uint8_t name[16];unsigned length;filename(io,name,&length);
        const unsigned device=io->read(io->ctx,0xba),secondary=io->read(io->ctx,0xb9);
        const int result=io->save(io->ctx,device,secondary,name,length,read16(io,0xc1),read16(io,0xae));
        if(!result)return 0;
        io->write(io->ctx,0x90,0);if(result<0){*next_pc=failure(result);return 1;}
        ++t->saves;*next_pc=0xf68d;return 1;
    }
    return 0;
}
void rc_traps_save(const rc_traps *t,rc_state_writer *w){rc_sw_u32(w,1);rc_sw_u8(w,t->known);rc_sw_u8(w,t->load_enabled);rc_sw_u8(w,t->save_enabled);rc_sw_u32(w,t->loads);rc_sw_u32(w,t->saves);}
int rc_traps_load(rc_traps *t,rc_state_reader *r){
    if(rc_sr_u32(r)!=1||rc_sr_u8(r)!=t->known)return 0;
    const unsigned load=rc_sr_u8(r),save=rc_sr_u8(r);if(load>1||save>1)return 0;
    t->loads=rc_sr_u32(r);t->saves=rc_sr_u32(r);if(!rc_sr_ok(r))return 0;rc_traps_enable(t,(int)load,(int)save);return 1;
}
