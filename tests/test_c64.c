#include "machine.h"
#include "../src/platform/png.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define CHECK(x) do{if(!(x)){fprintf(stderr,"line %d: %s\n",__LINE__,#x);return 1;}}while(0)
int main(void){
    rc_rom_index roms;rc_rom_index_init(&roms);const char *folders[]={RC_ROM_DIR};CHECK(rc_rom_index_scan(&roms,folders,1));
    const char *models[]={"c64-pal","c64c-pal","c64-ntsc","c64c-ntsc"};
    for(unsigned model=0;model<4;++model){
        rc_config config;rc_config_defaults(&config);snprintf(config.model,sizeof config.model,"%s",models[model]);
        char error[256],text[2048];rc_machine *m=rc_machine_create(&config,&roms,error,sizeof error);CHECK(m&&!strcmp(rc_machine_family(m)->id,"c64"));
        CHECK(rc_machine_run(m,rc_machine_frame_period(m)*200)==RC_RUN_OK);rc_machine_text(m,text,sizeof text);CHECK(strstr(text,"38911 BASIC BYTES FREE")&&strstr(text,"READY."));
        rc_frame f;CHECK(rc_machine_frame(m,&f));uint32_t *pixels=malloc((size_t)f.width*f.height*4);CHECK(pixels);rc_frame_argb(&f,pixels);
        char path[1024];snprintf(path,sizeof path,RC_WORK "/tests/%s.png",models[model]);CHECK(rc_png_write(path,pixels,(int)f.width,(int)f.height,(size_t)f.width*4,0));free(pixels);
        if(!model){
            const char *line="PRINT 3+4\n";
            for(;*line;++line){uint8_t key,shift;CHECK(rc_family_typing(rc_machine_family(m),*line,&key,&shift));if(shift!=RC_KEY_NONE)rc_machine_key(m,shift,1);rc_machine_key(m,key,1);rc_machine_run(m,rc_machine_frame_period(m)*3);rc_machine_key(m,key,0);if(shift!=RC_KEY_NONE)rc_machine_key(m,shift,0);rc_machine_run(m,rc_machine_frame_period(m)*3);}
            rc_machine_text(m,text,sizeof text);CHECK(strstr(text,"PRINT 3+4")&&strstr(text," 7"));
            rc_state_writer w;rc_sw_init(&w);rc_machine_save(m,&w);CHECK(rc_sw_finish(&w));
            uint8_t value;CHECK(rc_machine_peek(m,0xdc0d,&value)&&rc_machine_peek(m,0xd01e,&value));
            rc_state_writer peeked;rc_sw_init(&peeked);rc_machine_save(m,&peeked);CHECK(rc_sw_finish(&peeked));
            CHECK(w.size==peeked.size&&!memcmp(w.data,peeked.data,w.size));rc_sw_free(&peeked);
            rc_machine_run(m,12345);rc_state_writer expected;rc_sw_init(&expected);rc_machine_save(m,&expected);CHECK(rc_sw_finish(&expected));
            rc_state_reader r;CHECK(rc_sr_open(&r,w.data,w.size,error,sizeof error)&&rc_machine_load(m,&r,error,sizeof error));
            rc_machine_run(m,12345);rc_state_writer actual;rc_sw_init(&actual);rc_machine_save(m,&actual);CHECK(rc_sw_finish(&actual));
            CHECK(expected.size==actual.size&&!memcmp(expected.data,actual.data,actual.size));rc_sw_free(&w);rc_sw_free(&expected);rc_sw_free(&actual);
        }
        printf("%s READY and framebuffer OK\n",models[model]);rc_machine_destroy(m);
    }
    rc_rom_index_free(&roms);puts("C64 firmware boot, keyboard and state OK");return 0;
}
