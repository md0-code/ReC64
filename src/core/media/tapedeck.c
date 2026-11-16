#include "tapedeck.h"
#include <stdlib.h>
#include <string.h>

enum {SHORT=1,MEDIUM,LONG};
static unsigned pulse_class(uint32_t cycles){return cycles<200||cycles>900?0:cycles<456?SHORT:cycles<608?MEDIUM:LONG;}
static unsigned next_class(rc_tap *t){uint32_t cycles;return rc_tap_next(t,&cycles)?pulse_class(cycles):0;}
static int byte_at(rc_tap *t,uint8_t *byte){
    if(next_class(t)!=LONG||next_class(t)!=MEDIUM)return 0;
    unsigned value=0,ones=0;
    for(unsigned bit=0;bit<9;++bit){const unsigned a=next_class(t),b=next_class(t);unsigned v;
        if(a==SHORT&&b==MEDIUM)v=0;else if(a==MEDIUM&&b==SHORT)v=1;else return 0;
        ones+=v;if(bit<8)value|=v<<bit;
    }
    *byte=(uint8_t)value;return (ones&1u)!=0;
}
/* Search byte markers, remembering the pilot preceding the first sync byte. */
static int scan_byte(rc_tap *t,uint8_t *byte,size_t *pilot){
    unsigned short_run=0;size_t leader=t->position;
    while(t->position<t->image->size){const size_t at=t->position;const unsigned a=next_class(t);
        if(a==SHORT){if(!short_run)leader=at;++short_run;continue;}
        if(a==LONG){const size_t after=t->position;rc_tap candidate=*t;candidate.position=at;
            if(byte_at(&candidate,byte)){*pilot=short_run>=16?leader:at;*t=candidate;return 1;}
            t->position=after;
        }
        short_run=0;
    }
    return 0;
}
static int sync_at(rc_tap *t,unsigned *copy,size_t *pilot){
    unsigned expected=0;size_t start=t->position;uint8_t value;
    while(scan_byte(t,&value,pilot)){
        if(expected&&value==expected){
            if((expected&127u)==1){*copy=expected&128u;*pilot=start;return 1;}--expected;
        }else if(value==0x89||value==9){expected=value-1u;start=*pilot;}
        else expected=0;
    }
    return 0;
}
static int block_at(rc_tap *t,uint8_t *out,size_t length){
    uint8_t checksum=0,value;
    for(size_t i=0;i<length;++i){if(!byte_at(t,&value))return 0;checksum^=value;if(out)out[i]=value;}
    if(!byte_at(t,&value)||value!=checksum)return 0;
    /* End marker is not another byte: long followed by short. */
    return next_class(t)==LONG&&next_class(t)==SHORT;
}
static int skip_block(rc_tap *t,size_t length){
    uint32_t pulse;
    for(size_t i=0;i<(length+1)*20+2;++i)if(!rc_tap_next(t,&pulse))return 0;
    return 1;
}
typedef struct program {rc_tape_entry entry;size_t body;uint8_t header[192];} program;
static int next_program(rc_tap *t,program *p){
    unsigned copy;size_t pilot;
    while(sync_at(t,&copy,&pilot)){
        rc_tap candidate=*t;uint8_t header[192];if(!block_at(&candidate,header,sizeof header))continue;
        const unsigned address=rc_image_u16(header+1),end=rc_image_u16(header+3);
        const uint32_t length=end?(end>address?end-address:0):65536u-address;
        if((header[0]!=1&&header[0]!=3)||!length||length>65535)continue;
        memset(p,0,sizeof *p);memcpy(p->header,header,sizeof header);memcpy(p->entry.name,header+5,16);
        p->entry.address=(uint16_t)address;p->entry.length=length;p->entry.offset=pilot;
        *t=candidate;
        /* A first-copy header is followed by the repeated header. */
        if(copy){rc_tap repeat=*t;unsigned repeat_copy;size_t repeat_pilot;
            /* The first header is already verified. Its redundant copy may
             * itself have parity/checksum errors without losing the file. */
            if(sync_at(&repeat,&repeat_copy,&repeat_pilot)&&!repeat_copy&&skip_block(&repeat,sizeof header))*t=repeat;
        }
        p->body=t->position;p->entry.end_offset=t->position;return 1;
    }
    return 0;
}
/* Read the two body copies. A valid repeat recovers a damaged first copy. */
static int body_at(rc_tap *t,const program *p,uint8_t *out){
    int good=0;rc_tap cursor=*t;cursor.position=p->body;
    for(unsigned attempt=0;attempt<2;++attempt){unsigned copy;size_t pilot;
        if(!sync_at(&cursor,&copy,&pilot))break;
        if(attempt&&copy)break;
        rc_tap block=cursor;
        if(block_at(&block,good?NULL:out,p->entry.length)){good=1;cursor=block;}
        if(!copy)break;
    }
    *t=cursor;return good;
}
int rc_tape_directory(const rc_tap *t,rc_tape_entry *entries,size_t capacity,size_t *count,char *error,size_t cap){
    if(count)*count=0;if(!t||!t->image)return rc_image_error(error,cap,"No TAP image");
    rc_tap cursor=*t;cursor.position=20;program p;size_t total=0;
    while(next_program(&cursor,&p)){
        body_at(&cursor,&p,NULL);p.entry.end_offset=cursor.position;
        if(total<capacity&&entries)entries[total]=p.entry;++total;
    }
    if(count)*count=total;return 1;
}
int rc_tape_extract(const rc_tap *t,unsigned ordinal,uint16_t *address,uint8_t **data,size_t *size,char *error,size_t cap){
    if(data)*data=NULL;if(size)*size=0;if(!t||!t->image||!data)return rc_image_error(error,cap,"No TAP image");
    rc_tap cursor=*t;cursor.position=20;program p;unsigned index=0;
    while(next_program(&cursor,&p)){
        if(index++==ordinal){uint8_t *bytes=malloc(p.entry.length);if(!bytes)return rc_image_error(error,cap,"Out of memory");
            if(!body_at(&cursor,&p,bytes)){free(bytes);return rc_image_error(error,cap,"TAP program has no valid KERNAL data block");}
            *data=bytes;if(size)*size=p.entry.length;if(address)*address=p.entry.address;return 1;
        }
        body_at(&cursor,&p,NULL);
    }
    return rc_image_error(error,cap,"TAP program not found");
}
typedef struct encoder {uint8_t *data;size_t size,capacity;int failed;} encoder;
static void emit(encoder *e,unsigned pulse){
    if(e->failed)return;
    if(e->size==e->capacity){const size_t capacity=e->capacity?e->capacity*2:65536;
        if(capacity>RC_MEDIA_LIMIT){e->failed=1;return;}uint8_t *next=realloc(e->data,capacity);if(!next){e->failed=1;return;}e->data=next;e->capacity=capacity;}
    e->data[e->size++]=(uint8_t)pulse;
}
static void write_byte(encoder *e,uint8_t value){
    emit(e,86);emit(e,66);unsigned parity=1;
    for(unsigned bit=0;bit<9;++bit){const unsigned v=bit<8?(value>>bit)&1u:parity;parity^=v;
        emit(e,v?66u:48u);emit(e,v?48u:66u);}
}
static void write_block(encoder *e,const uint8_t *data,size_t size,unsigned leader,int first){
    for(unsigned i=0;i<leader;++i)emit(e,48);
    for(unsigned value=9;value;--value)write_byte(e,(uint8_t)(value|(first?128u:0)));
    uint8_t checksum=0;for(size_t i=0;i<size;++i){checksum^=data[i];write_byte(e,data[i]);}write_byte(e,checksum);
    emit(e,86);emit(e,48);
}
int rc_tape_encode(rc_tap *out,uint16_t address,const uint8_t *data,size_t size,const uint8_t name[16],char *error,size_t cap){
    if(!out||!data||!size||size>65535||size>65536u-address)return rc_image_error(error,cap,"Invalid tape program address or length");
    encoder e={0};for(unsigned i=0;i<20;++i)emit(&e,0);
    uint8_t header[192];memset(header,32,sizeof header);header[0]=(uint8_t)(address==0x0801?1:3);rc_image_put16(header+1,address);rc_image_put16(header+3,(uint16_t)(address+size));
    if(name)memcpy(header+5,name,16);
    write_block(&e,header,sizeof header,0x6a00,1);write_block(&e,header,sizeof header,0x4f,0);
    write_block(&e,data,size,0x1500,1);write_block(&e,data,size,0x4f,0);
    for(unsigned i=0;i<64;++i)emit(&e,48);
    if(e.failed){free(e.data);return rc_image_error(error,cap,"Cannot allocate TAP pulse stream");}
    memcpy(e.data,"C64-TAPE-RAW",12);e.data[12]=1;rc_image_put32(e.data+16,(uint32_t)(e.size-20));
    const int ok=rc_tap_open(out,e.data,e.size,error,cap);free(e.data);return ok;
}
void rc_tape_deck_init(rc_tape_deck *d){memset(d,0,sizeof *d);}
void rc_tape_deck_eject(rc_tape_deck *d){if(d->owned)rc_tap_close(&d->tape);memset(d,0,sizeof *d);}
int rc_tape_deck_insert(rc_tape_deck *d,rc_media *media,char *error,size_t cap){
    if(!media)return rc_image_error(error,cap,"No TAP image");rc_tap checked;
    if(!rc_tap_open(&checked,media->data,media->size,error,cap))return 0;
    rc_media_close(checked.image);checked.image=media;rc_tape_deck_eject(d);d->tape=checked;return 1;
}
int rc_tape_deck_seek(rc_tape_deck *d,unsigned ordinal){
    if(!d->tape.image)return 0;size_t count=0;if(!rc_tape_directory(&d->tape,NULL,0,&count,NULL,0)||ordinal>=count)return 0;
    rc_tape_entry *entries=malloc(count*sizeof *entries);if(!entries)return 0;
    const int ok=rc_tape_directory(&d->tape,entries,count,&count,NULL,0);
    if(ok){d->tape.position=entries[ordinal].offset;d->eof=0;}free(entries);return ok;
}
int rc_tape_deck_next(rc_tape_deck *d,int direction){
    if(!d->tape.image)return 0;size_t count=0;if(!rc_tape_directory(&d->tape,NULL,0,&count,NULL,0)||!count)return 0;
    rc_tape_entry *entries=malloc(count*sizeof *entries);if(!entries)return 0;
    const int ok=rc_tape_directory(&d->tape,entries,count,&count,NULL,0);size_t selected=0;
    if(ok){while(selected+1<count&&entries[selected+1].offset<=d->tape.position)++selected;
        if(direction>0&&selected+1<count)++selected;else if(direction<0&&selected)--selected;
        d->tape.position=entries[selected].offset;d->eof=0;}
    free(entries);return ok;
}
void rc_tape_deck_save(const rc_tape_deck *d,rc_state_writer *w){
    rc_sw_u32(w,1);rc_sw_u8(w,d->play);rc_sw_u8(w,d->record);rc_sw_u8(w,d->eof);rc_sw_u8(w,d->tape.image!=NULL);
    if(d->tape.image)rc_tap_save(&d->tape,w);
}
int rc_tape_deck_load(rc_tape_deck *d,rc_state_reader *r){
    if(rc_sr_u32(r)!=1)return 0;const uint8_t play=rc_sr_u8(r),record=rc_sr_u8(r),eof=rc_sr_u8(r),mounted=rc_sr_u8(r);
    if(play>1||record>1||eof>1||mounted>1||(!mounted&&(play||record))||(record&&!play))return 0;rc_tap next={0};if(mounted&&!rc_tap_load(&next,r))return 0;
    if(!rc_sr_ok(r)){rc_tap_close(&next);return 0;}
    rc_media *borrowed=d->owned?NULL:d->tape.image;
    if(mounted&&borrowed){free(borrowed->data);*borrowed=*next.image;free(next.image);next.image=borrowed;d->tape=next;d->owned=0;}
    else {rc_tape_deck_eject(d);d->tape=next;d->owned=mounted;}
    d->play=play;d->record=record;d->eof=eof;return 1;
}
