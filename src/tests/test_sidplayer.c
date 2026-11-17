/* One focused format/driver/player and machine-state continuation check. */
#include "../core/machine.h"
#include "../app/sidplayer.h"
#include "../core/palette.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define CHECK(x) do{if(!(x)){fprintf(stderr,"line %d: %s (%s)\n",__LINE__,#x,error);return 1;}}while(0)
static void be16(uint8_t *p,unsigned value){p[0]=(uint8_t)(value>>8);p[1]=(uint8_t)value;}
static size_t fixture(uint8_t *d,int mode){
    const int rsid=mode==1||mode==3;memset(d,0,4096);memcpy(d,rsid?"RSID":"PSID",4);be16(d+4,2);be16(d+6,124);be16(d+10,mode==3?0:0x1000);
    be16(d+12,mode?0:0x1040);be16(d+14,2);be16(d+16,1);be16(d+118,mode==3?0x16:0x14);d[124]=mode==3?1:0;d[125]=mode==3?8:0x10;
    memcpy(d+22,"ORIGINAL DRIVER CHECK",21);memcpy(d+54,"ReC64",5);memcpy(d+86,"2026",4);
    const uint8_t init[]={0xa9,0x0f,0x8d,0x18,0xd4,0xa9,0x18,0x8d,0x01,0xd4,0xa9,0xf0,0x8d,0x06,0xd4,0xa9,0x21,0x8d,0x04,0xd4};
    unsigned offset=126+(mode==3?0x1000-0x0801:0);
    if(mode==3){const uint8_t basic[]={0x0b,0x08,10,0,0x9e,'4','0','9','6',0,0,0};memcpy(d+126,basic,sizeof basic);}
    memcpy(d+offset,init,sizeof init);unsigned at=offset+(unsigned)sizeof init;
    if(mode){const uint8_t irq_install[]={0xa9,0x50,0x8d,0x14,0x03,0xa9,0x10,0x8d,0x15,0x03};memcpy(d+at,irq_install,sizeof irq_install);at+=(unsigned)sizeof irq_install;}
    d[at]=0x60;const uint8_t play[]={0xee,0x00,0xd4,0x60};memcpy(d+offset+0x40,play,sizeof play);
    const uint8_t irq[]={0xee,0x00,0xd4,0xad,0x0d,0xdc,0xa9,1,0x8d,0x19,0xd0,0x68,0xa8,0x68,0xaa,0x68,0x40};memcpy(d+offset+0x50,irq,sizeof irq);
    return offset+0x50+sizeof irq;
}
static size_t startup_fixture(uint8_t *d){
    const size_t size=fixture(d,0);
    /* Song 1 explicitly mutes; song 2 leaves volume to the player. Both
     * spend about 1/6 second initializing, as a packed tune can do. */
    const uint8_t init[]={0xaa,0xd0,5,0xa9,0,0x8d,0x18,0xd4,
        0xa2,0,0xa0,128,0xca,0xd0,0xfd,0x88,0xd0,0xfa,
        0xa9,0x18,0x8d,0x01,0xd4,0xa9,0xf0,0x8d,0x06,0xd4,0xa9,0x21,0x8d,0x04,0xd4,0x60};
    memcpy(d+126,init,sizeof init);return size;
}
static int save(rc_machine *m,rc_state_writer *w){rc_sw_init(w);rc_machine_save(m,w);return rc_sw_finish(w);}
int main(int argc,char **argv){
    char error[256]="";uint8_t data[4096];size_t size=fixture(data,0);rc_sidfile file;
    CHECK(rc_sidfile_parse(&file,data,size,error,sizeof error)&&file.load==0x1000&&file.songs==2&&file.clock==RC_SID_PAL&&file.model==RC_SID_MODEL_6581);
    rc_sid_driver driver;CHECK(rc_sidfile_driver(&file,1,&driver,error,sizeof error)&&driver.address<file.load&&driver.length<256);
    data[122]=0x42;be16(data+4,3);CHECK(!rc_sidfile_parse(&file,data,size,error,sizeof error)&&strstr(error,"Multiple"));
    size=fixture(data,0);be16(data+14,64);data[21]=1;CHECK(rc_sidfile_parse(&file,data,size,error,sizeof error));CHECK(!rc_sidfile_cia(&file,32));
    be16(data+118,0x16);CHECK(rc_sidfile_parse(&file,data,size,error,sizeof error)&&rc_sidfile_cia(&file,32));
    size=fixture(data,0);rc_sidplayer player;rc_sidplayer_init(&player);CHECK(rc_sidplayer_open(&player,data,size,error,sizeof error));
    rc_frame frame;rc_sidplayer_draw(&player,1,985248u*65u,985248,1,rc_palette_builtin("color-corrected"),&frame);char text[2048];rc_sidplayer_text(&player,text,sizeof text);
    CHECK(strstr(text,"ORIGINAL DRIVER CHECK")&&strstr(text,"01:05  PAUSED")&&strstr(text,"SUB-TUNE 2 / 2"));CHECK(rc_sidplayer_key(&player,"Right",1,0)==RC_SIDPLAYER_NONE);
    rc_rom_index roms;rc_rom_index_init(&roms);const char *folders[]={argc>1?argv[1]:RC_ROM_DIR};CHECK(rc_rom_index_scan(&roms,folders,1));
    rc_config cfg;rc_config_defaults(&cfg);snprintf(cfg.model,sizeof cfg.model,"c64-pal");rc_machine *m=rc_machine_create(&cfg,&roms,error,sizeof error);CHECK(m);
    for(int mode=0;mode<4;++mode){
        size=fixture(data,mode);rc_media *media=rc_media_from("original-check.sid",data,size);CHECK(media&&rc_machine_insert(m,4,media,error,sizeof error));
        CHECK(rc_machine_run(m,rc_machine_clock(m)*3u)==RC_RUN_OK);int16_t audio[4096];size_t got=rc_machine_audio_pull(m,audio,2048);int audible=0;for(size_t i=0;i<got*2;++i)if(audio[i]>64||audio[i]<-64)audible=1;CHECK(audible);
        rc_state_writer before,expected,actual;CHECK(save(m,&before));CHECK(rc_machine_run(m,12345)==RC_RUN_OK&&save(m,&expected));rc_state_reader r;
        CHECK(rc_sr_open(&r,before.data,before.size,error,sizeof error)&&rc_machine_load(m,&r,error,sizeof error));CHECK(rc_machine_run(m,12345)==RC_RUN_OK&&save(m,&actual));
        CHECK(expected.size==actual.size&&!memcmp(expected.data,actual.data,actual.size));rc_sw_free(&before);rc_sw_free(&expected);rc_sw_free(&actual);
        uint64_t time=rc_machine_time(m);CHECK(rc_machine_control(m,4,RC_MEDIA_PAUSE,1)==2&&rc_machine_run(m,10000)==RC_RUN_OK&&rc_machine_time(m)==time);
        CHECK(rc_machine_control(m,4,RC_MEDIA_PAUSE,0)==1&&rc_machine_control(m,4,RC_MEDIA_SONG,1)==2);CHECK(rc_machine_eject(m,4));rc_media_close(media);
    }
    size=startup_fixture(data);rc_media *media=rc_media_from("startup-check.sid",data,size);CHECK(media&&rc_machine_insert(m,4,media,error,sizeof error));
    for(unsigned song=0;song<2;++song){
        CHECK(rc_machine_control(m,4,RC_MEDIA_SONG,song)==(int)song+1);
        rc_machine_status status;do{CHECK(rc_machine_run(m,256)==RC_RUN_OK);rc_machine_get_status(m,&status);}while(!status.sid_cycles);
        int16_t audio[4096];size_t got;while(rc_machine_audio_pull(m,audio,2048)){}
        CHECK(rc_machine_run(m,4096)==RC_RUN_OK);got=rc_machine_audio_pull(m,audio,2048);CHECK(got);
        for(size_t i=0;i<got*2;++i)CHECK(audio[i]==0); /* no full-volume idle transient during init */
        rc_state_writer before,expected,actual;CHECK(save(m,&before));CHECK(rc_machine_run(m,200000)==RC_RUN_OK&&save(m,&expected));rc_state_reader r;
        CHECK(rc_sr_open(&r,before.data,before.size,error,sizeof error)&&rc_machine_load(m,&r,error,sizeof error));
        CHECK(rc_machine_run(m,200000)==RC_RUN_OK&&save(m,&actual));CHECK(expected.size==actual.size&&!memcmp(expected.data,actual.data,actual.size));
        int audible=0;while((got=rc_machine_audio_pull(m,audio,2048))>0)for(size_t i=0;i<got*2;++i)if(audio[i]>64||audio[i]<-64)audible=1;
        CHECK(audible==(song==1));rc_sw_free(&before);rc_sw_free(&expected);rc_sw_free(&actual);
    }
    CHECK(rc_machine_eject(m,4));rc_media_close(media);
    rc_machine_destroy(m);rc_rom_index_free(&roms);puts("SID parser, IRQ playback, quiet initialization, volume fallback and exact state OK");return 0;
}
