#include "datasette.h"
#include "../core/machine.h"
#include <string.h>
static void read_line(rc_datasette *s,int level){
    if(s->read==(uint8_t)level)return;s->read=(uint8_t)level;
    if(s->dev.host&&s->dev.host->line)rc_dev_line(&s->dev,RC_DATASETTE_READ,level);
}
static void reset(void *self){rc_datasette *s=self;s->motor=0;s->write=1;s->record_cycles=0;s->have_record_edge=s->failed=0;read_line(s,1);}
static void save_dev(const void *self,rc_state_writer *w){rc_datasette_save(self,w);}
static int load_dev(void *self,rc_state_reader *r){return rc_datasette_load(self,r);}
static const rc_dev_ops OPS={.reset=reset,.save=save_dev,.load=load_dev};
void rc_datasette_init(rc_datasette *s,const rc_hw_host *host){memset(s,0,sizeof *s);s->dev=(rc_dev){&OPS,host,"Datasette"};s->read=s->write=s->auto_play=1;}
int rc_datasette_insert(rc_datasette *s,rc_media *media,char *error,size_t cap){
    if(!rc_tape_deck_insert(&s->deck,media,error,cap))return 0;s->remaining=s->pulse=s->record_cycles=0;s->have_record_edge=s->failed=0;read_line(s,1);return 1;
}
void rc_datasette_eject(rc_datasette *s){rc_tape_deck_eject(&s->deck);s->remaining=s->pulse=s->record_cycles=0;s->have_record_edge=0;read_line(s,1);}
int rc_datasette_control(rc_datasette *s,unsigned command,uint32_t value){
    rc_tape_deck *d=&s->deck;if(!d->tape.image)return 0;
    switch(command){
    case RC_MEDIA_PLAY:d->play=1;d->record=0;break;
    case RC_MEDIA_STOP:d->play=d->record=0;break;
    case RC_MEDIA_REWIND:rc_tap_rewind(&d->tape);d->eof=0;s->remaining=s->pulse=0;read_line(s,1);break;
    case RC_MEDIA_NEXT:case RC_MEDIA_PREVIOUS:if(!rc_tape_deck_next(d,command==RC_MEDIA_NEXT?1:-1))return 0;s->remaining=s->pulse=0;read_line(s,1);break;
    case RC_MEDIA_SEEK:if(!rc_tape_deck_seek(d,value))return 0;s->remaining=s->pulse=0;read_line(s,1);break;
    case RC_MEDIA_RECORD:
        if(!d->tape.image->writable)return 0;
        /* Recording replaces the stream from the current tape position. */
        if(!rc_media_resize(d->tape.image,d->tape.position))return 0;
        rc_image_put32(d->tape.image->data+16,(uint32_t)(d->tape.image->size-20));d->tape.image->dirty=1;
        d->play=d->record=1;d->eof=0;s->remaining=s->pulse=s->record_cycles=0;s->have_record_edge=s->failed=0;read_line(s,1);break;
    default:return 0;
    }
    if(!d->record){s->record_cycles=0;s->have_record_edge=0;}return 1;
}
void rc_datasette_auto_play(rc_datasette *s,int enabled){s->auto_play=(uint8_t)(enabled!=0);}
void rc_datasette_motor(rc_datasette *s,int on){
    const int started=on&&!s->motor;s->motor=(uint8_t)(on!=0);
    if(started&&s->auto_play&&s->deck.tape.image&&s->deck.tape.image->size>20&&!s->deck.eof&&!s->deck.record)s->deck.play=1;
}
void rc_datasette_write(rc_datasette *s,int high){
    high=high!=0;
    /* The C2N record/read path inverts polarity: a rising WRITE edge is
     * the falling READ edge whose period a TAP stores. */
    if(s->motor&&s->deck.play&&s->deck.record&&!s->write&&high){
        if(s->have_record_edge){char error[128];if(!rc_tap_append(&s->deck.tape,s->record_cycles,error,sizeof error))s->failed=1;else s->deck.tape.position=s->deck.tape.image->size;}
        s->record_cycles=0;s->have_record_edge=1;
    }
    s->write=(uint8_t)high;
}
void rc_datasette_tick(rc_datasette *s,uint32_t cycles){
    if(!s->motor||!s->deck.play||!s->deck.tape.image||s->failed)return;
    if(s->deck.record){if(s->have_record_edge){if(cycles>0xffffffu-s->record_cycles)s->failed=1;else s->record_cycles+=cycles;}return;}
    while(cycles){
        if(!s->remaining){
            if(!rc_tap_next(&s->deck.tape,&s->pulse)){s->deck.eof=1;s->deck.play=0;read_line(s,1);return;}
            s->remaining=s->pulse;
        }
        /* TAP stores complete falling-edge periods, not half-waves. */
        uint32_t until=s->remaining;
        if(!s->read&&s->remaining>s->pulse/2)until=s->remaining-s->pulse/2;
        const uint32_t step=cycles<until?cycles:until;s->remaining-=step;cycles-=step;
        if(!s->remaining){if(!s->read)read_line(s,1);read_line(s,0);}
        else if(!s->read&&s->remaining<=s->pulse/2)read_line(s,1);
    }
}
int rc_datasette_sense(const rc_datasette *s){return !s->deck.play;}
int rc_datasette_read(const rc_datasette *s){return s->read;}
int16_t rc_datasette_sample(const rc_datasette *s){return s->motor&&s->deck.play?(int16_t)((s->deck.record?s->write:s->read)?2048:-2048):0;}
int rc_datasette_ok(const rc_datasette *s){return !s->failed;}
void rc_datasette_save(const rc_datasette *s,rc_state_writer *w){
    rc_sw_u32(w,1);rc_tape_deck_save(&s->deck,w);rc_sw_u32(w,s->remaining);rc_sw_u32(w,s->pulse);rc_sw_u32(w,s->record_cycles);
    rc_sw_u8(w,s->motor);rc_sw_u8(w,s->write);rc_sw_u8(w,s->read);rc_sw_u8(w,s->have_record_edge);rc_sw_u8(w,s->auto_play);rc_sw_u8(w,s->failed);
}
int rc_datasette_load(rc_datasette *s,rc_state_reader *r){
    if(rc_sr_u32(r)!=1||!rc_tape_deck_load(&s->deck,r))return 0;
    s->remaining=rc_sr_u32(r);s->pulse=rc_sr_u32(r);s->record_cycles=rc_sr_u32(r);
    s->motor=rc_sr_u8(r);s->write=rc_sr_u8(r);s->read=rc_sr_u8(r);s->have_record_edge=rc_sr_u8(r);s->auto_play=rc_sr_u8(r);s->failed=rc_sr_u8(r);
    return rc_sr_ok(r)&&s->pulse<=0xffffffu&&s->remaining<=s->pulse&&s->record_cycles<=0xffffffu&&s->motor<=1&&s->write<=1&&s->read<=1&&s->have_record_edge<=1&&s->auto_play<=1&&s->failed<=1;
}
