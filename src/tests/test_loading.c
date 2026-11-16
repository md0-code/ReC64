/* Actual KERNAL loading, fast/original paths and tape SAVE round trip. */
#include "../core/machines/c64.h"
#include "../core/machines/c64_media.h"
#include "../core/media/prg.h"
#include "../core/media/t64.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define CHECK(x) do{if(!(x)){fprintf(stderr,"line %d: %s PC=%04X source=%u motor=%u play=%u pos=%zu ICR=%02X status=%02X\n%s\n",__LINE__,#x,c?c->cpu.pc:0,c?c->source:0,c?c->tape.motor:0,c?c->tape.deck.play:0,c?c->tape.deck.tape.position:0,c?c->cia[0].icr:0,c?c->bus.ram[0x90]:0,error);return 1;}}while(0)
static const uint8_t PROGRAM[]={0x14,0x08,10,0,0x99,0x20,0x22,'P','H','A','S','E',' ','F','O','U','R',0x22,0,0,0};
static uint64_t period(const rc_c64 *c){return rc_vic2_cycles_per_line(&c->vic)*rc_vic2_height(&c->vic);}
static int ready(rc_c64 *c){char text[2048];const size_t n=rc_c64_family.impl->text(c,text,sizeof text);size_t at=n;while(at&&(text[at-1]=='\n'||text[at-1]==' '))--at;return at>=6&&!memcmp(text+at-6,"READY.",6);}
static int type(rc_c64 *c,const char *text){for(;*text;++text){uint8_t key,shift;if(!rc_family_typing(&rc_c64_family,(unsigned char)*text,&key,&shift))return 0;if(shift!=RC_KEY_NONE)rc_c64kbd_key(&c->keyboard,shift,1);rc_c64kbd_key(&c->keyboard,key,1);if(rc_c64_run(c,period(c)*3)!=RC_RUN_OK)return 0;rc_c64kbd_key(&c->keyboard,key,0);if(shift!=RC_KEY_NONE)rc_c64kbd_key(&c->keyboard,shift,0);if(rc_c64_run(c,period(c)*3)!=RC_RUN_OK)return 0;}return 1;}
int main(int argc,char **argv){
    rc_c64 *c=NULL;char error[256]="";const char *kind=argc>1?argv[1]:"prg";const int fast=argc>2?atoi(argv[2]):1;
    rc_rom_index roms;rc_rom_index_init(&roms);const char *folders[]={RC_ROM_DIR};CHECK(rc_rom_index_scan(&roms,folders,1));
    unsigned options[]={0};rc_machine_setup setup={.family=&rc_c64_family,.model=0,.options=options,.roms=&roms};c=rc_c64_create(&setup,error,sizeof error);CHECK(c);CHECK(rc_c64_run(c,period(c)*200)==RC_RUN_OK&&ready(c));
    const uint8_t name[16]={'P','H','A','S','E','4',' ',' ',' ',' ',' ',' ',' ',' ',' ',' '};rc_media *media=NULL;
    if(!strcmp(kind,"prg")||!strcmp(kind,"fastsave")){rc_prg p={0};CHECK(rc_prg_create(&p,0x0801,PROGRAM,sizeof PROGRAM,name,0,error,sizeof error));media=p.image;snprintf(media->path,sizeof media->path,"fixture.prg");}
    else if(!strcmp(kind,"t64")){rc_t64 t={0};const rc_t64_program p={.address=0x0801,.data=PROGRAM,.size=sizeof PROGRAM};CHECK(rc_t64_create(&t,&p,1,NULL,error,sizeof error));media=t.image;snprintf(media->path,sizeof media->path,"fixture.t64");}
    else if(!strcmp(kind,"tap")||!strcmp(kind,"save")){rc_tap t={0};if(!strcmp(kind,"save"))CHECK(rc_tap_create(&t,1,error,sizeof error));else CHECK(rc_tape_encode(&t,0x0801,PROGRAM,sizeof PROGRAM,name,error,sizeof error));media=t.image;snprintf(media->path,sizeof media->path,"fixture.tap");}
    else CHECK(0);
    const unsigned slot=!strcmp(kind,"prg")||!strcmp(kind,"fastsave")?1u:0u;CHECK(rc_c64_insert(c,slot,media,error,sizeof error));
    rc_c64_tape_options(c,slot,RC_TAPE_SOUND|(fast?RC_TAPE_TRAPS|RC_TAPE_SAVE_TRAPS|RC_TAPE_ACCELERATE:0));
    if(!strcmp(kind,"tap")&&!fast){
        rc_c64_family.impl->reset(c,1);CHECK(rc_c64_run(c,period(c)*200)==RC_RUN_OK&&ready(c));
        CHECK(c->tape.deck.tape.position==20&&!rc_c64_activity(c));
    }
    if(!strcmp(kind,"save")){
        memcpy(c->bus.ram+0x0801,PROGRAM,sizeof PROGRAM);rc_put16(c->bus.ram+0x2b,0x0801);rc_put16(c->bus.ram+0x2d,(uint16_t)(0x0801+sizeof PROGRAM));
        CHECK(rc_c64_control(c,0,RC_MEDIA_RECORD,0));CHECK(type(c,"SAVE\"PHASE4\",1\n"));
    }else CHECK(type(c,"LOAD\"\",1,1\n"));
    unsigned frames=0;while(!ready(c)&&frames++<6000){CHECK(rc_c64_run(c,period(c))==RC_RUN_OK);
        if(!strcmp(kind,"tap")&&!fast&&frames==300){
            rc_state_writer saved,expected,actual;rc_sw_init(&saved);rc_sw_header(&saved,"c64","c64-pal",NULL,0);rc_c64_family.impl->save(c,&saved);CHECK(rc_sw_finish(&saved));
            CHECK(rc_c64_run(c,12345)==RC_RUN_OK);rc_sw_init(&expected);rc_sw_header(&expected,"c64","c64-pal",NULL,0);rc_c64_family.impl->save(c,&expected);CHECK(rc_sw_finish(&expected));
            rc_state_reader r;CHECK(rc_sr_open(&r,saved.data,saved.size,error,sizeof error)&&rc_c64_family.impl->load(c,&r));CHECK(rc_c64_run(c,12345)==RC_RUN_OK);
            rc_sw_init(&actual);rc_sw_header(&actual,"c64","c64-pal",NULL,0);rc_c64_family.impl->save(c,&actual);CHECK(rc_sw_finish(&actual));
            CHECK(expected.size==actual.size&&!memcmp(expected.data,actual.data,actual.size));rc_sw_free(&saved);rc_sw_free(&expected);rc_sw_free(&actual);
            int16_t samples[1024];const size_t count=rc_c64_family.impl->audio_pull(c,samples,512);int heard=0;for(size_t i=0;i<count*2;++i)if(samples[i])heard=1;CHECK(count&&heard);
            puts("Tape board partial-state and audible loading signal OK");
        }
    }
    if(!ready(c)){char text[2048];rc_c64_family.impl->text(c,text,sizeof text);fprintf(stderr,"%s\n",text);}CHECK(ready(c));
    if(!strcmp(kind,"save")){
        CHECK(rc_c64_control(c,0,RC_MEDIA_STOP,0));
        FILE *recorded=fopen(RC_WORK "/p4-loading/recorded.tap","wb");CHECK(recorded);CHECK(fwrite(media->data,1,media->size,recorded)==media->size);CHECK(!fclose(recorded));
        rc_tap t={0};CHECK(rc_tap_open(&t,media->data,media->size,error,sizeof error));uint16_t address;uint8_t *data=NULL;size_t length;CHECK(rc_tape_extract(&t,0,&address,&data,&length,error,sizeof error));CHECK(address==0x0801&&length==sizeof PROGRAM&&!memcmp(data,PROGRAM,length));free(data);rc_tap_close(&t);
        CHECK(rc_c64_control(c,0,RC_MEDIA_REWIND,0));memset(c->bus.ram+0x0801,0,sizeof PROGRAM);CHECK(type(c,"LOAD\"\",1,1\n"));frames=0;while(!ready(c)&&frames++<6000)CHECK(rc_c64_run(c,period(c))==RC_RUN_OK);CHECK(ready(c));
    }
    uint8_t expected_program[sizeof PROGRAM];memcpy(expected_program,PROGRAM,sizeof PROGRAM);
    if(!strcmp(kind,"fastsave")){
        expected_program[14]='I';expected_program[15]='V';expected_program[16]='E';memcpy(c->bus.ram+0x0801,expected_program,sizeof PROGRAM);
        CHECK(type(c,"SAVE\"\",1\n")&&c->traps.saves==1);memset(c->bus.ram+0x0801,0,sizeof PROGRAM);
        CHECK(type(c,"LOAD\"\",1,1\n"));frames=0;while(!ready(c)&&frames++<6000)CHECK(rc_c64_run(c,period(c))==RC_RUN_OK);CHECK(ready(c));
    }
    CHECK(!memcmp(c->bus.ram+0x0801,expected_program,sizeof PROGRAM));CHECK(type(c,"RUN\n"));char text[2048];rc_c64_family.impl->text(c,text,sizeof text);CHECK(strstr(text,!strcmp(kind,"fastsave")?"PHASE FIVE":"PHASE FOUR"));
    printf("%s %s loaded identical program and ran; cycles=%llu trap_loads=%u tape_bytes=%zu\n",kind,fast?"fast":"original",(unsigned long long)c->sched.now,c->traps.loads,media->size);
    rc_c64_family.impl->destroy(c);rc_media_close(media);rc_rom_index_free(&roms);return 0;
}
