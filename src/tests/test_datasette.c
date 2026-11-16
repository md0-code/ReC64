#include "datasette.h"
#include "../core/machine.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define CHECK(x) do{if(!(x)){fprintf(stderr,"line %d: %s\n",__LINE__,#x);return 1;}}while(0)
typedef struct host {uint64_t time,last;unsigned falls;uint32_t first_period;} host;
static void line(void *ctx,void *dev,int pin,int level){host *h=ctx;(void)dev;if(pin==RC_DATASETTE_READ&&!level){if(!h->falls)h->first_period=(uint32_t)h->time;h->last=h->time;++h->falls;}}
static void tick(rc_datasette *s,host *h,unsigned cycles){while(cycles--){++h->time;rc_datasette_tick(s,1);}}
static void save(rc_datasette *s,rc_state_writer *w){rc_sw_init(w);rc_sw_header(w,"test","datasette",NULL,0);rc_sw_begin(w,"TAPE",0);rc_datasette_save(s,w);rc_sw_end(w);rc_sw_finish(w);}
int main(void){
    const uint8_t bytes[]={0x0d,0x08,10,0,0x99,0x22,'P','4','C',0x22,0,0,0};uint8_t name[16];memset(name,32,sizeof name);memcpy(name,"P4C TAPE",8);
    char error[128];rc_tap tap={0};CHECK(rc_tape_encode(&tap,0x0801,bytes,sizeof bytes,name,error,sizeof error));
    rc_tape_entry entry;size_t count=0;CHECK(rc_tape_directory(&tap,&entry,1,&count,error,sizeof error)&&count==1&&entry.address==0x0801&&entry.length==sizeof bytes&&entry.offset==20&&!memcmp(entry.name,name,16));
    uint8_t *decoded=NULL;size_t length=0;uint16_t address;CHECK(rc_tape_extract(&tap,0,&address,&decoded,&length,error,sizeof error)&&address==0x0801&&length==sizeof bytes&&!memcmp(bytes,decoded,length));free(decoded);
    /* A bad checksum/parity in the redundant header must not discard a
     * verified first header and two good copies of the program data. */
    const size_t repeat_checksum=20+0x6a00+(9+192+1)*20+2+0x4f+(9+192)*20;
    uint8_t *pair=tap.image->data+repeat_checksum+2;const uint8_t a=pair[0],b=pair[1];pair[0]=b;pair[1]=a;
    CHECK(rc_tape_extract(&tap,0,&address,&decoded,&length,error,sizeof error)&&length==sizeof bytes&&!memcmp(bytes,decoded,length));free(decoded);pair[0]=a;pair[1]=b;
    FILE *file=fopen(RC_WORK "/tests/p4c-synthetic.tap","wb");CHECK(file);CHECK(fwrite(tap.image->data,1,tap.image->size,file)==tap.image->size);fclose(file);
    host h={0};rc_hw_host callbacks={.ctx=&h,.line=line};rc_datasette s;rc_datasette_init(&s,&callbacks);CHECK(rc_datasette_insert(&s,tap.image,error,sizeof error));CHECK(rc_datasette_sense(&s));
    CHECK(rc_datasette_control(&s,RC_MEDIA_PLAY,0)&&!rc_datasette_sense(&s));rc_datasette_motor(&s,1);tick(&s,&h,384);CHECK(h.falls==1&&h.first_period==384&&!rc_datasette_read(&s));tick(&s,&h,192);CHECK(rc_datasette_read(&s));
    rc_datasette_motor(&s,0);const uint32_t remaining=s.remaining;tick(&s,&h,1000);CHECK(s.remaining==remaining&&h.falls==1&&!rc_datasette_sample(&s));rc_datasette_motor(&s,1);
    rc_state_writer before,expected,actual;save(&s,&before);CHECK(!before.failed);tick(&s,&h,1234);save(&s,&expected);rc_state_reader reader;CHECK(rc_sr_open(&reader,before.data,before.size,error,sizeof error)&&rc_sr_chunk(&reader,"TAPE",0)&&rc_datasette_load(&s,&reader));tick(&s,&h,1234);save(&s,&actual);CHECK(expected.size==actual.size&&!memcmp(expected.data,actual.data,actual.size));rc_sw_free(&before);rc_sw_free(&expected);rc_sw_free(&actual);
    CHECK(rc_datasette_control(&s,RC_MEDIA_STOP,0));rc_datasette_motor(&s,1);CHECK(rc_datasette_sense(&s));CHECK(rc_datasette_control(&s,RC_MEDIA_SEEK,0)&&s.deck.tape.position==20);rc_datasette_eject(&s);rc_tap_close(&tap);
    /* A recorded TAP uses complete write falling-edge periods. */
    CHECK(rc_tap_create(&tap,1,error,sizeof error));CHECK(rc_datasette_insert(&s,tap.image,error,sizeof error));CHECK(rc_datasette_control(&s,RC_MEDIA_RECORD,0));rc_datasette_motor(&s,1);rc_datasette_write(&s,0);tick(&s,&h,384);rc_datasette_write(&s,1);rc_datasette_write(&s,0);tick(&s,&h,528);rc_datasette_write(&s,1);rc_datasette_write(&s,0);CHECK(rc_datasette_control(&s,RC_MEDIA_STOP,0)&&rc_datasette_ok(&s));rc_tap_rewind(&tap);uint32_t pulse;CHECK(rc_tap_next(&tap,&pulse)&&pulse==384&&rc_tap_next(&tap,&pulse)&&pulse==528&&!rc_tap_next(&tap,&pulse));rc_datasette_eject(&s);rc_tap_close(&tap);
    /* Version 0 long pulses and motor stops retain exact playback position. */
    CHECK(rc_tap_create(&tap,0,error,sizeof error)&&rc_tap_append(&tap,384,error,sizeof error)&&rc_tap_append(&tap,2048,error,sizeof error));CHECK(rc_datasette_insert(&s,tap.image,error,sizeof error));rc_datasette_auto_play(&s,0);CHECK(rc_datasette_control(&s,RC_MEDIA_PLAY,0));h.time=h.last=h.falls=0;rc_datasette_motor(&s,1);tick(&s,&h,384+2048);CHECK(h.falls==2&&h.last==384+2048);rc_datasette_eject(&s);rc_tap_close(&tap);
    puts("KERNAL tape codec, Datasette pulse timing, recording, transport and state OK");return 0;
}
