#include "traps.h"
#include "../app/typist.h"
#include "romdb.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define CHECK(x) do{if(!(x)){fprintf(stderr,"line %d: %s\n",__LINE__,#x);return 1;}}while(0)
typedef struct fixture {uint8_t ram[65536],program[4];unsigned reads,writes,calls;int result;uint16_t saved_start,saved_end;} fixture;
static uint8_t read_byte(void *ctx,uint16_t at){fixture *f=ctx;++f->reads;return f->ram[at];}
static void write_byte(void *ctx,uint16_t at,uint8_t value){fixture *f=ctx;++f->writes;f->ram[at]=value;}
static int load_file(void *ctx,unsigned device,unsigned secondary,const uint8_t name[16],unsigned length,uint16_t *address,const uint8_t **data,size_t *size){
    fixture *f=ctx;(void)device;(void)secondary;(void)name;(void)length;++f->calls;*address=0x801;*data=f->program;*size=sizeof f->program;return f->result;
}
static int save_file(void *ctx,unsigned device,unsigned secondary,const uint8_t name[16],unsigned length,uint16_t start,uint16_t end){
    fixture *f=ctx;(void)device;(void)secondary;(void)name;(void)length;f->saved_start=start;f->saved_end=end;return f->result;
}
int main(void){
    const char *hex=rc_romdb_find("kernal")->sha256;uint8_t hash[32];
    for(unsigned i=0;i<32;++i){char pair[]={hex[i*2],hex[i*2+1],0};hash[i]=(uint8_t)strtoul(pair,NULL,16);}
    fixture *f=calloc(1,sizeof *f);CHECK(f);memcpy(f->program,"ABCD",4);f->result=1;f->ram[0xba]=1;f->ram[0xc3]=0;f->ram[0xc4]=0x20;f->ram[0xf4a5]=0x85;
    const rc_traps_io io={f,read_byte,write_byte,load_file,save_file};rc_traps traps;rc_traps_init(&traps,hash);CHECK(traps.known);
    uint16_t pc=0;CHECK(rc_traps_fetch(&traps,&io,0xf4a5,0,&pc)&&pc==0xf5a9);CHECK(!memcmp(f->ram+0x2000,"ABCD",4)&&f->ram[0xae]==4&&f->ram[0xaf]==0x20&&f->ram[0x93]==0);
    f->result=2;f->ram[0x801]=0;CHECK(rc_traps_fetch(&traps,&io,0xf4a5,0,&pc)&&!memcmp(f->ram+0x801,"ABCD",4));f->result=1;
    f->ram[0xb9]=1;CHECK(rc_traps_fetch(&traps,&io,0xf4a5,0,&pc)&&!memcmp(f->ram+0x801,"ABCD",4));
    f->ram[0x802]='X';CHECK(rc_traps_fetch(&traps,&io,0xf4a5,1,&pc)&&f->ram[0x90]==0x10&&f->ram[0x802]=='X');
    CHECK(f->ram[0xf4a5]==0x85); /* No trap opcode or firmware patch. */
    const unsigned writes=f->writes;f->result=0;CHECK(!rc_traps_fetch(&traps,&io,0xf4a5,0,&pc)&&f->writes==writes);
    f->result=-4;CHECK(rc_traps_fetch(&traps,&io,0xf4a5,0,&pc)&&pc==0xf704);
    f->result=1;f->ram[0xc1]=1;f->ram[0xc2]=8;f->ram[0xae]=5;f->ram[0xaf]=8;CHECK(rc_traps_fetch(&traps,&io,0xf5ed,0,&pc)&&pc==0xf68d&&f->saved_start==0x801&&f->saved_end==0x805);
    rc_state_writer w;rc_sw_init(&w);rc_sw_header(&w,"test","traps",NULL,0);rc_sw_begin(&w,"TRAP",0);rc_traps_save(&traps,&w);rc_sw_end(&w);CHECK(rc_sw_finish(&w));
    const rc_traps expected=traps;rc_traps_enable(&traps,0,0);const unsigned calls=f->calls;CHECK(!rc_traps_fetch(&traps,&io,0xf4a5,0,&pc)&&f->calls==calls&&!traps.watch[0xf4a5u>>3]);
    rc_state_reader r;char error[256];CHECK(rc_sr_open(&r,w.data,w.size,error,sizeof error)&&rc_sr_chunk(&r,"TRAP",0)&&rc_traps_load(&traps,&r)&&!memcmp(&expected,&traps,sizeof traps));rc_sw_free(&w);
    hash[0]^=1;rc_traps_init(&traps,hash);rc_traps_enable(&traps,1,1);const unsigned reads=f->reads;CHECK(!traps.known&&!rc_traps_fetch(&traps,&io,0xf4a5,0,&pc)&&f->reads==reads);free(f);
    rc_rom_index roms;rc_rom_index_init(&roms);const char *folders[]={RC_ROM_DIR};CHECK(rc_rom_index_scan(&roms,folders,1));rc_config config;rc_config_defaults(&config);
    rc_machine *m=rc_machine_create(&config,&roms,error,sizeof error);CHECK(m&&!strcmp(rc_machine_family(m)->id,"c64"));const uint64_t frame=rc_machine_frame_period(m);CHECK(rc_machine_run(m,frame*200)==RC_RUN_OK);
    rc_keymap keys;rc_keymap_init(&keys,m,&config);rc_typist typist;CHECK(rc_typist_start(&typist,"PRINT 2+3\\nPRINT 6+7\\n",error,sizeof error));
    unsigned frames=0;while(rc_typist_busy(&typist)&&frames++<8){CHECK(rc_typist_frame(&typist,m,&keys)>=0);CHECK(rc_machine_run(m,frame)==RC_RUN_OK);rc_keymap_frame(&keys);}
    CHECK(!rc_typist_busy(&typist)&&!keys.count);char text[2048];rc_machine_text(m,text,sizeof text);CHECK(strstr(text,"PRINT 2+3")&&strstr(text,"\n 5\n")&&strstr(text,"PRINT 6+7")&&strstr(text,"\n 13\n"));
    rc_machine_destroy(m);rc_rom_index_free(&roms);puts("Known-ROM LOAD/SAVE hooks, VERIFY, bypass, state and prompt-driven typing OK");return 0;
}
