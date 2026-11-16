/* Original C64/1541 firmware through the serial bus, plus a public disk loader. */
#include "../core/machines/c64.h"
#include "../core/machines/c64_media.h"
#include "../platform/png.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static rc_c64 *c;static char error[256];
static void report(void){char text[2048];if(!c)return;rc_c64_family.impl->text(c,text,sizeof text);fprintf(stderr,"C64=%04X drive=%04X job=%02X IEC=%X PA=%02X PB=%02X track=%u motor=%u\n%s\n%s\n",c->cpu.pc,c->drive.cpu.pc,c->drive.ram[0],rc_iec_lines(&c->iec),c->cia[1].port[0],c->drive.via[0].port[1],c->drive.halftrack,c->drive.motor,error,text);}
#define CHECK(x) do{if(!(x)){fprintf(stderr,"line %d: %s\n",__LINE__,#x);report();return 1;}}while(0)
static uint64_t period(void){return rc_vic2_cycles_per_line(&c->vic)*rc_vic2_height(&c->vic);}
static int ready(void){char text[2048];size_t n=rc_c64_family.impl->text(c,text,sizeof text);while(n&&(text[n-1]=='\n'||text[n-1]==' '))--n;return n>=6&&!memcmp(text+n-6,"READY.",6);}
static int type(const char *text){for(;*text;++text){uint8_t key,shift;if(!rc_family_typing(&rc_c64_family,(unsigned char)*text,&key,&shift))return 0;if(shift!=RC_KEY_NONE)rc_c64kbd_key(&c->keyboard,shift,1);rc_c64kbd_key(&c->keyboard,key,1);if(rc_c64_run(c,period()*3)!=RC_RUN_OK)return 0;rc_c64kbd_key(&c->keyboard,key,0);if(shift!=RC_KEY_NONE)rc_c64kbd_key(&c->keyboard,shift,0);if(rc_c64_run(c,period()*3)!=RC_RUN_OK)return 0;}return 1;}
static int prompt(unsigned frames){while(!ready()&&frames--)if(rc_c64_run(c,period())!=RC_RUN_OK)return 0;return ready();}
static int shot(const char *name){rc_frame f;if(!rc_c64_family.impl->frame(c,&f))return 0;uint32_t *pixels=malloc((size_t)f.width*f.height*4);if(!pixels)return 0;rc_frame_argb(&f,pixels);char path[1024];snprintf(path,sizeof path,RC_WORK "/p5/%s.png",name);int ok=rc_png_write(path,pixels,(int)f.width,(int)f.height,(size_t)f.width*4,0);free(pixels);return ok;}
int main(int argc,char **argv){
    const int loader=argc>1&&!strcmp(argv[1],"loader");rc_rom_index roms;rc_rom_index_init(&roms);const char *folders[]={RC_ROM_DIR};CHECK(rc_rom_index_scan(&roms,folders,1));
    unsigned options[RC_CONFIG_OPTIONS]={0};rc_machine_setup setup={.family=&rc_c64_family,.model=0,.options=options,.roms=&roms};c=rc_c64_create(&setup,error,sizeof error);CHECK(c);CHECK(rc_c64_run(c,period()*200)==RC_RUN_OK&&ready());
    rc_media *media=NULL;
    if(loader){media=rc_media_open(RC_WORK "/p5/loadertest-c64.d64",0,error,sizeof error);CHECK(media);}
    else {rc_d64 disk;uint8_t name[16];memset(name,0xa0,16);memcpy(name,"PHASE FIVE",10);const uint8_t id[2]={'P','5'};CHECK(rc_d64_create(&disk,name,id,error,sizeof error));media=disk.image;snprintf(media->path,sizeof media->path,RC_WORK "/p5/saved.d64");}
    CHECK(rc_c64_insert(c,2,media,error,sizeof error));
    if(loader){
        CHECK(type("LOAD\"*\",8,1\n")&&prompt(18000));CHECK(type("RUN\n"));unsigned ram_frames=0;
        for(unsigned frame=0;frame<6000;++frame){CHECK(rc_c64_run(c,period())==RC_RUN_OK);if(c->drive.cpu.pc<0x800)++ram_frames;if(frame==999||frame==2999||frame==5999){char name[32];snprintf(name,sizeof name,"loader-%u",frame+1);CHECK(shot(name));report();}}
        CHECK(ram_frames>50);printf("Custom drive code observed for %u frames; cycles=%llu\n",ram_frames,(unsigned long long)c->sched.now);
    }else {
        CHECK(type("LOAD\"$\",8\n")&&prompt(4000));CHECK(type("LIST\n"));char text[2048];rc_c64_family.impl->text(c,text,sizeof text);CHECK(strstr(text,"PHASE FIVE")&&strstr(text,"BLOCKS FREE"));CHECK(shot("directory"));
        CHECK(type("NEW\n10PRINT\"PHASE FIVE\"\nSAVE\"PHASE5\",8\n")&&prompt(10000));CHECK(rc_c64_control(c,2,RC_MEDIA_FLUSH,0)&&media->dirty);CHECK(rc_media_save(media));
        rc_d64 disk;CHECK(rc_d64_open(&disk,media->data,media->size,error,sizeof error));rc_d64_entry entry;size_t count;CHECK(rc_d64_directory(&disk,&entry,1,&count,error,sizeof error)&&count==1);uint8_t *saved=NULL;size_t size;CHECK(rc_d64_extract(&disk,&entry,&saved,&size,error,sizeof error)&&size>10);rc_d64_close(&disk);
        CHECK(type("NEW\nLOAD\"PHASE5\",8\n")&&prompt(10000));CHECK(!memcmp(c->bus.ram+rc_get16(saved),saved+2,size-2));free(saved);CHECK(type("RUN\n"));rc_c64_family.impl->text(c,text,sizeof text);CHECK(strstr(text,"PHASE FIVE"));CHECK(shot("saved-loadback"));
        CHECK(c->traps.loads==0&&c->traps.saves==0);printf("Actual serial directory, disk SAVE and LOAD/RUN passed; cycles=%llu\n",(unsigned long long)c->sched.now);
    }
    rc_c64_family.impl->destroy(c);rc_media_close(media);rc_rom_index_free(&roms);return 0;
}
