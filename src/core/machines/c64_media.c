/* Board media wiring, original KERNAL interfaces and format readers. */
#include "c64_media.h"
#include "c64_expansion.h"
#include "../media/prg.h"
#include "../media/t64.h"
#include "../media/d64.h"
#include <ctype.h>
#include <stdlib.h>
#include <string.h>

static unsigned letter(unsigned c){return c>=0xc1&&c<=0xda?c-128:c>=97&&c<=122?c-32:c==0xa0?' ':c;}
static void filename(uint8_t out[16],const char *path){
    const char *name=path;for(const char *p=path;*p;++p)if(*p=='/'||*p=='\\')name=p+1;
    memset(out,' ',16);for(unsigned i=0;i<16&&name[i]&&name[i]!='.';++i)out[i]=(uint8_t)letter((unsigned char)name[i]);
}
static int matches(const uint8_t pattern[16],unsigned length,const uint8_t name[16]){
    if(!length)return 1;unsigned n=16;while(n&&letter(name[n-1])==' ')--n;
    for(unsigned i=0;i<length&&i<16;++i){const unsigned ch=letter(pattern[i]);if(ch=='*')return 1;if(i>=n||(ch!='?'&&ch!=letter(name[i])))return 0;}
    return length==n;
}
static int select_entry(rc_c64 *c,unsigned slot,unsigned entry,char *error,size_t cap){
    rc_media *m=c->media[slot];if(!m)return rc_image_error(error,cap,"No image inserted");
    if(slot==2){
        if(entry==UINT32_MAX){memset(c->disk_name,0,16);c->disk_address=0x0801;c->selected[2]=entry;return 1;}
        rc_d64 d={0};if(!rc_d64_open(&d,m->data,m->size,error,cap))return 0;
        size_t count=0;int ok=rc_d64_directory(&d,NULL,0,&count,error,cap);rc_d64_entry *entries=ok&&count?calloc(count,sizeof *entries):NULL;
        if(!entries||!rc_d64_directory(&d,entries,count,&count,error,cap)||entry>=count||(entries[entry].type&7u)!=2)ok=0;
        if(ok){const rc_d64_entry *e=&entries[entry];const uint8_t *sector=rc_d64_sector(&d,e->track,e->sector);
            if(!sector)ok=0;else {memcpy(c->disk_name,e->name,16);c->disk_address=rc_get16(sector+2);c->selected[2]=entry;}}
        free(entries);rc_d64_close(&d);if(!ok)return rc_image_error(error,cap,"Select a PRG file from the disk directory");return 1;
    }
    if(rc_media_extension_in(m->path,"tap")){
        if(!rc_datasette_insert(&c->tape,m,error,cap))return 0;
        if(entry!=UINT32_MAX&&!rc_datasette_control(&c->tape,RC_MEDIA_SEEK,entry))return rc_image_error(error,cap,"Tape entry not found");
        c->program_address=0x0801;size_t count=0;
        if(rc_tape_directory(&c->tape.deck.tape,NULL,0,&count,NULL,0)&&count){rc_tape_entry *entries=calloc(count,sizeof *entries);
            if(entries&&rc_tape_directory(&c->tape.deck.tape,entries,count,&count,NULL,0)){const unsigned index=entry==UINT32_MAX?0:entry;if(index<count)c->program_address=entries[index].address;}free(entries);}
        rc_media_close(c->synthetic);c->synthetic=NULL;free(c->program);c->program=NULL;c->program_size=0;
        c->source=(uint8_t)slot;c->physical=1;c->selected[slot]=entry==UINT32_MAX?0:entry;rc_c64_media_lines(c);return 1;
    }
    uint8_t *data=NULL,name[16];uint16_t address=0;size_t size=0;filename(name,m->path);int ok=0;
    if(rc_media_extension_in(m->path,"prg,p00")){
        rc_prg p={0};if(rc_prg_open(&p,m->data,m->size,rc_media_extension_in(m->path,"p00"),error,cap)){
            address=p.address;size=p.length;data=malloc(size?size:1);if(data){memcpy(data,rc_prg_data(&p),size);if(p.p00)memcpy(name,p.name,16);ok=1;}else rc_image_error(error,cap,"Out of memory");rc_prg_close(&p);}
    }else if(rc_media_extension_in(m->path,"t64")){
        rc_t64 t={0};if(rc_t64_open(&t,m->data,m->size,error,cap)){rc_t64_entry e;
            if(rc_t64_entry_at(&t,entry==UINT32_MAX?0:entry,&e)){address=e.address;size=e.length;data=malloc(size?size:1);if(data){memcpy(data,t.image->data+e.offset,size);memcpy(name,e.name,16);ok=1;}}
            if(!ok)rc_image_error(error,cap,"T64 entry not found");rc_t64_close(&t);}
    }
    if(!ok){free(data);return 0;}for(unsigned i=0;i<16;++i)name[i]=(uint8_t)letter(name[i]);
    rc_tap tape={0};if(!rc_tape_encode(&tape,address,data,size,name,error,cap)){free(data);return 0;}
    tape.image->writable=0;snprintf(tape.image->path,sizeof tape.image->path,"%s",m->path);
    if(!rc_datasette_insert(&c->tape,tape.image,error,cap)){rc_tap_close(&tape);free(data);return 0;}
    rc_media_close(c->synthetic);c->synthetic=tape.image;free(c->program);c->program=data;c->program_size=(uint32_t)size;
    c->program_address=address;memcpy(c->program_name,name,16);c->source=(uint8_t)slot;c->physical=0;c->selected[slot]=entry==UINT32_MAX?0:entry;
    rc_c64_media_lines(c);return 1;
}
static uint8_t read_byte(void *self,uint16_t a){return rc_bus_read(&((rc_c64 *)self)->bus,a,1);}
static void write_byte(void *self,uint16_t a,uint8_t v){rc_bus_write(&((rc_c64 *)self)->bus,a,v);}
static int trap_load(void *self,unsigned device,unsigned secondary,const uint8_t name[16],unsigned length,uint16_t *address,const uint8_t **data,size_t *size){
    (void)secondary;rc_c64 *c=self;if(c->physical||c->source>=2||!c->program||device!=1)return 0;
    if(!matches(name,length,c->program_name)){
        rc_media *m=c->media[c->source];int found=0;
        if(m&&rc_media_extension_in(m->path,"t64")){rc_t64 t={0};char error[128];if(rc_t64_open(&t,m->data,m->size,error,sizeof error)){
            for(unsigned i=0;i<t.count;++i){rc_t64_entry e;if(rc_t64_entry_at(&t,i,&e)&&matches(name,length,e.name)){found=select_entry(c,c->source,i,error,sizeof error);break;}}rc_t64_close(&t);}}
        if(!found)return -4;
    }
    *address=c->program_address;*data=c->program;*size=c->program_size;return device==1&&c->program_address!=0x0801?2:1;
}
static int trap_save(void *self,unsigned device,unsigned secondary,const uint8_t name[16],unsigned length,uint16_t start,uint16_t end){
    (void)secondary;(void)length;rc_c64 *c=self;if(c->physical||c->source>=2||device!=1)return 0;rc_media *m=c->media[c->source];
    if(!m||!rc_media_extension_in(m->path,"prg,p00"))return 0;if(!m->writable)return -5;
    const size_t size=(uint16_t)(end-start);uint8_t *bytes=malloc(size?size:1);if(!bytes)return -5;for(size_t i=0;i<size;++i)bytes[i]=read_byte(c,(uint16_t)(start+i));
    rc_prg p={0};char error[128];const int ok=rc_prg_create(&p,start,bytes,size,name,rc_media_extension_in(m->path,"p00"),error,sizeof error);free(bytes);if(!ok)return -5;
    const unsigned slot=c->source;snprintf(p.image->path,sizeof p.image->path,"%s",m->path);c->media[slot]=p.image;
    const int selected=select_entry(c,slot,c->selected[slot],error,sizeof error);c->media[slot]=m;
    if(!selected){rc_prg_close(&p);return -5;}
    free(m->data);m->data=p.image->data;m->size=p.image->size;m->dirty=1;p.image->data=NULL;rc_prg_close(&p);return 1;
}
static int before_fetch(void *self,uint16_t pc){
    rc_c64 *c=self;if(!(rc_mos6510_port_read(&c->cpu,1)&2u)||!c->bus.game||!c->bus.exrom)return 0;
    if(pc==0xf817&&c->tape.deck.tape.image&&c->tape.deck.tape.image->size>20&&!c->tape.deck.record){rc_datasette_control(&c->tape,RC_MEDIA_PLAY,0);rc_c64_media_lines(c);}
    const unsigned bits=c->source<3?c->tape_bits[c->source]:0;
    rc_traps_enable(&c->traps,!c->physical&&(bits&RC_TAPE_TRAPS),!c->physical&&(bits&RC_TAPE_SAVE_TRAPS));
    const rc_traps_io io={c,read_byte,write_byte,trap_load,trap_save};uint16_t next=pc;
    if(!rc_traps_fetch(&c->traps,&io,pc,c->cpu.a,&next))return 0;c->cpu.pc=next;return 1;
}
void rc_c64_media_init(rc_c64 *c){
    rc_datasette_init(&c->tape,&c->host);rc_datasette_auto_play(&c->tape,0);rc_traps_init(&c->traps,c->rom_hashes);c->source=255;
    for(unsigned i=0;i<3;++i)c->tape_bits[i]=RC_TAPE_SOUND|RC_TAPE_TRAPS|RC_TAPE_SAVE_TRAPS|RC_TAPE_ACCELERATE;
    if(c->traps.known){const unsigned addresses[]={0xf4a5,0xf5ed,0xf817};for(unsigned i=0;i<3;++i)c->fetch_watch[addresses[i]>>3]|=(uint8_t)(1u<<(addresses[i]&7));}
    c->hooks=(rc_cpu_hooks){c,c->fetch_watch,before_fetch,NULL};c->cpu.hooks=&c->hooks;
}
void rc_c64_media_lines(rc_c64 *c){
    rc_mos6510_port_input(&c->cpu,(uint8_t)(7u|c->port_charge|(rc_datasette_sense(&c->tape)?16u:0)));
    const uint8_t port=rc_mos6510_port_read(&c->cpu,1);rc_datasette_motor(&c->tape,!(port&32u));rc_datasette_write(&c->tape,(port&8u)!=0);
}
void rc_c64_iec_lines(rc_c64 *c){
    /* The inverting buffers remain connected when PA3..5 become inputs:
     * their pull-ups then pull the corresponding IEC line low. */
    const unsigned pa=rc_cia6526_port_output(&c->cia[1],0);
    const unsigned output=RC_IEC_RESET|(!(pa&8u)?RC_IEC_ATN:0u)|(!(pa&16u)?RC_IEC_CLK:0u)|(!(pa&32u)?RC_IEC_DATA:0u);
    rc_iec_output(&c->iec,0,output);rc_c1541_iec_input(&c->drive,rc_iec_lines(&c->iec));
    rc_iec_output(&c->iec,1,rc_c1541_iec_output(&c->drive));rc_c1541_iec_input(&c->drive,rc_iec_lines(&c->iec));
    const unsigned lines=rc_iec_lines(&c->iec);
    rc_cia6526_port_input(&c->cia[1],0,(uint8_t)(63u|((lines&RC_IEC_CLK)?64u:0u)|((lines&RC_IEC_DATA)?128u:0u)));
}
void rc_c64_media_tick(rc_c64 *c){
    rc_c64_iec_lines(c);c->drive_phase+=1000000u;
    while(c->drive_phase>=c->hz){c->drive_phase-=c->hz;rc_c1541_tick(&c->drive);rc_c64_iec_lines(c);}
    rc_fastload_observe(&c->fastload,c->sched.now,c->drive.motor,c->iec.edges);
    if((c->drive.motor!=0)!=c->drive_sound.motor||c->drive.steps!=c->drive_sound.steps||c->drive.bumps!=c->drive_sound.bumps)
        rc_drivesnd_status(&c->drive_sound,c->drive.motor,c->drive.steps,c->drive.bumps);
    rc_c64_media_lines(c);rc_datasette_tick(&c->tape,1);c->tape_audio_phase+=48000;
    if(c->tape_audio_phase>=c->hz){c->tape_audio_phase-=c->hz;
        if(c->tape_audio_count==48000){c->tape_audio_head=(c->tape_audio_head+1)%48000;--c->tape_audio_count;}
        int sample=(c->source<2&&(c->tape_bits[c->source]&RC_TAPE_SOUND))?rc_datasette_sample(&c->tape):0;
        /* A tune is copied into memory by the player, not loaded from the drive. */
        const int drive=rc_drivesnd_sample(&c->drive_sound);if((c->tape_bits[2]&RC_TAPE_SOUND)&&!c->sid_session.active)sample+=drive;
        if(sample>32767)sample=32767;if(sample< -32768)sample= -32768;
        c->tape_audio[(c->tape_audio_head+c->tape_audio_count++)%48000]=(int16_t)sample;
    }
}
int16_t rc_c64_tape_audio(rc_c64 *c){if(!c->tape_audio_count)return 0;const int16_t value=c->tape_audio[c->tape_audio_head];c->tape_audio_head=(c->tape_audio_head+1)%48000;--c->tape_audio_count;return value;}
void rc_c64_media_release(rc_c64 *c){rc_c64_sid_eject(c);rc_c1541_close(&c->drive);rc_datasette_eject(&c->tape);rc_media_close(c->synthetic);c->synthetic=NULL;free(c->program);c->program=NULL;for(unsigned i=0;i<5;++i){if(c->media_owned[i])rc_media_close(c->media[i]);c->media[i]=NULL;c->media_owned[i]=0;}c->source=255;}
int rc_c64_insert(void *self,unsigned slot,rc_media *image,char *error,size_t cap){
    rc_c64 *c=self;if(slot>=5||!image)return rc_image_error(error,cap,"Invalid media slot");
    const char *formats[]={"tap,t64","prg,p00","d64,g64","crt","sid"};if(!rc_media_extension_in(image->path,formats[slot]))return rc_image_error(error,cap,"Wrong image type for this slot");
    rc_media *old=c->media[slot];const uint8_t owned=c->media_owned[slot];c->media[slot]=image;c->media_owned[slot]=0;
    int ok=1;if(slot==2){ok=rc_c1541_mount(&c->drive,image,error,cap);if(ok)ok=select_entry(c,slot,UINT32_MAX,error,cap);}
    else if(slot==3){ok=rc_c64_cart_insert(c,image,error,cap);if(ok)c->selected[3]=UINT32_MAX;}
    else if(slot==4){ok=rc_c64_sid_insert(c,image,error,cap);if(ok)c->selected[4]=UINT32_MAX;}
    else ok=select_entry(c,slot,UINT32_MAX,error,cap);
    if(!ok){c->media[slot]=old;c->media_owned[slot]=owned;return 0;}if(owned)rc_media_close(old);return 1;
}
rc_media *rc_c64_media(const void *self,unsigned slot){return slot<5?((const rc_c64 *)self)->media[slot]:NULL;}
int rc_c64_eject(void *self,unsigned slot){rc_c64 *c=self;if(slot>=5||!c->media[slot])return 0;
    if(slot==2&&!rc_c1541_mount(&c->drive,NULL,NULL,0))return 0;
    if(slot==3&&!rc_c64_cart_insert(c,NULL,NULL,0))return 0;
    if(slot==4)rc_c64_sid_eject(c);
    if(c->source==slot){rc_datasette_eject(&c->tape);rc_media_close(c->synthetic);c->synthetic=NULL;free(c->program);c->program=NULL;c->program_size=0;c->source=255;rc_c64_media_lines(c);}
    if(c->media_owned[slot])rc_media_close(c->media[slot]);c->media[slot]=NULL;c->media_owned[slot]=0;return 1;
}
int rc_c64_control(void *self,unsigned slot,unsigned command,uint32_t value){rc_c64 *c=self;if(slot>=5)return 0;
    if(slot==4)return command==RC_MEDIA_FLUSH?1:rc_c64_sid_control(c,command,value);
    if(command==RC_MEDIA_FREEZE)return slot==3&&rc_c64_cart_freeze(c,value);
    if(command==RC_MEDIA_FLUSH)return slot==2?rc_c1541_flush(&c->drive):slot==3?rc_c64_cart_flush(c):1;
    if(slot==3)return 0;
    if(command==RC_MEDIA_SEEK&&c->media[slot]){char error[128];return select_entry(c,slot,value,error,sizeof error);}
    if((command==RC_MEDIA_NEXT||command==RC_MEDIA_PREVIOUS||command==RC_MEDIA_REWIND)&&c->media[slot]&&rc_media_extension_in(c->media[slot]->path,"t64")){
        rc_t64 t={0};char error[128];if(!rc_t64_open(&t,c->media[slot]->data,c->media[slot]->size,error,sizeof error))return 0;
        unsigned entry=c->selected[slot];if(command==RC_MEDIA_REWIND)entry=0;else if(command==RC_MEDIA_NEXT&&entry+1<t.count)++entry;else if(command==RC_MEDIA_PREVIOUS&&entry)--entry;
        rc_t64_close(&t);const int playing=c->tape.deck.play,ok=select_entry(c,slot,entry,error,sizeof error);if(ok&&playing)rc_datasette_control(&c->tape,RC_MEDIA_PLAY,0);return ok;
    }
    if(slot!=0&&slot!=c->source)return 0;const int ok=rc_datasette_control(&c->tape,command,value);rc_c64_media_lines(c);return ok;
}
unsigned rc_c64_activity(const void *self){const rc_c64 *c=self;return (c->source<2&&c->tape.motor&&c->tape.deck.play?1u<<c->source:0)|(rc_fastload_active(&c->fastload)?4u:0);}
unsigned rc_c64_tape_options(void *self,unsigned slot,unsigned bits){rc_c64 *c=self;if(slot>=3)return 0;if(!(bits&RC_TAPE_QUERY))c->tape_bits[slot]=bits&15u;return c->tape_bits[slot];}
const char *rc_c64_autoload(const void *self,unsigned slot,int *power){rc_c64 *c=(rc_c64 *)self;*power=1;if(slot>=3)return NULL;
    if(slot==2){
        unsigned n=16;while(n&&c->disk_name[n-1]==0xa0)--n;if(!c->disk_name[0])n=0;
        int simple=1;for(unsigned i=0;i<n;++i)if(c->disk_name[i]<32||c->disk_name[i]>95||c->disk_name[i]=='"')simple=0;
        size_t used=0;if(simple)used=(size_t)snprintf(c->autoload_text,sizeof c->autoload_text,"LOAD\"%.*s\",%u,1\n",(int)(n?n:1),n?(const char *)c->disk_name:"*",c->drive.device);
        else {used=(size_t)snprintf(c->autoload_text,sizeof c->autoload_text,"A$=\"\"\n");for(unsigned i=0;i<n&&used<sizeof c->autoload_text;++i)used+=(size_t)snprintf(c->autoload_text+used,sizeof c->autoload_text-used,"A$=A$+CHR$(%u)\n",c->disk_name[i]);
            if(used<sizeof c->autoload_text)used+=(size_t)snprintf(c->autoload_text+used,sizeof c->autoload_text-used,"LOADA$,%u,1\n",c->drive.device);}
        if(used<sizeof c->autoload_text){if(c->disk_address==0x0801||!c->disk_address)snprintf(c->autoload_text+used,sizeof c->autoload_text-used,"RUN\n");else snprintf(c->autoload_text+used,sizeof c->autoload_text-used,"SYS%u\n",c->disk_address);}return c->autoload_text;
    }
    if(c->program_address==0x0801||!c->program_address)snprintf(c->autoload_text,sizeof c->autoload_text,"LOAD\"\",1,1\nRUN\n");
    else snprintf(c->autoload_text,sizeof c->autoload_text,"LOAD\"\",1,1\nSYS%u\n",c->program_address);return c->autoload_text;
}
struct rc_tape_deck *rc_c64_tape_deck(void *self,unsigned slot){rc_c64 *c=self;return (slot==0||slot==c->source)&&c->tape.deck.tape.image?&c->tape.deck:NULL;}
void rc_c64_media_save(const rc_c64 *c,rc_state_writer *w){
    rc_sw_begin(w,"CMED",0);rc_sw_u32(w,4);rc_sw_u8(w,c->source);rc_sw_u8(w,c->physical);
    rc_sw_u64(w,c->drive_phase);rc_sw_bytes(w,c->disk_name,16);rc_sw_u16(w,c->disk_address);
    for(unsigned i=0;i<5;++i){rc_sw_u32(w,c->selected[i]);rc_sw_u32(w,c->tape_bits[i]);rc_sw_u8(w,c->media[i]!=NULL);if(c->media[i])rc_image_save(c->media[i],w);}
    rc_sw_u16(w,c->program_address);rc_sw_bytes(w,c->program_name,16);rc_sw_packed(w,c->program,c->program_size);
    rc_sw_u8(w,c->synthetic!=NULL);if(c->synthetic)rc_image_save(c->synthetic,w);
    rc_sw_u64(w,c->tape_audio_phase);rc_sw_u32(w,c->tape_audio_count);for(unsigned i=0;i<c->tape_audio_count;++i)rc_sw_u16(w,(uint16_t)c->tape_audio[(c->tape_audio_head+i)%48000]);rc_sw_end(w);
    rc_sw_begin(w,"DSET",0);rc_datasette_save(&c->tape,w);rc_sw_end(w);rc_sw_begin(w,"TRAP",0);rc_traps_save(&c->traps,w);rc_sw_end(w);
    rc_sw_begin(w,"1541",0);rc_c1541_save(&c->drive,w);rc_sw_end(w);rc_sw_begin(w,"IEC ",0);rc_iec_save(&c->iec,w);rc_sw_end(w);
    rc_sw_begin(w,"FAST",0);rc_fastload_save(&c->fastload,w);rc_sw_end(w);
    rc_sw_begin(w,"DSND",0);rc_drivesnd_save(&c->drive_sound,w);rc_sw_end(w);
}
int rc_c64_media_load(rc_c64 *c,rc_state_reader *r){
    if(!rc_sr_chunk(r,"CMED",0))return 1;const unsigned version=rc_sr_u32(r);if(version<2||version>4)return 0;
    const unsigned source=rc_sr_u8(r),physical=rc_sr_u8(r);if((source>=3&&source!=255)||physical>1)return 0;
    c->drive_phase=rc_sr_u64(r);rc_sr_bytes(r,c->disk_name,16);c->disk_address=rc_sr_u16(r);if(c->drive_phase>=c->hz)return 0;
    if(!rc_c1541_mount(&c->drive,NULL,NULL,0))return 0;
    if(!rc_c64_cart_insert(c,NULL,NULL,0))return 0;
    rc_c64_sid_eject(c);
    rc_datasette_eject(&c->tape);rc_media_close(c->synthetic);c->synthetic=NULL;free(c->program);c->program=NULL;c->program_size=0;
    for(unsigned i=0;i<(version>=4?5u:version>=3?4u:3u);++i){c->selected[i]=rc_sr_u32(r);c->tape_bits[i]=rc_sr_u32(r);const unsigned present=rc_sr_u8(r);if(present>1||c->tape_bits[i]>15)return 0;
        rc_media *next=present?rc_image_load(r):NULL;if(present&&!next)return 0;rc_media *old=c->media[i];
        if(next&&old&&!c->media_owned[i]&&!strcmp(next->path,old->path)){free(old->data);*old=*next;free(next);next=old;}
        else {if(c->media_owned[i])rc_media_close(old);c->media_owned[i]=(uint8_t)(next!=NULL);}c->media[i]=next;
    }
    c->program_address=rc_sr_u16(r);rc_sr_bytes(r,c->program_name,16);
    if(r->end-r->at<4)return 0;c->program_size=rc_get16(r->data+r->at)|(uint32_t)rc_get16(r->data+r->at+2)<<16;if(c->program_size>65536)return 0;
    c->program=malloc(c->program_size?c->program_size:1);if(!c->program)return 0;rc_sr_packed(r,c->program,c->program_size);
    const unsigned synthetic=rc_sr_u8(r);if(synthetic>1)return 0;if(synthetic){c->synthetic=rc_image_load(r);if(!c->synthetic)return 0;}
    c->tape_audio_phase=rc_sr_u64(r);c->tape_audio_head=0;c->tape_audio_count=rc_sr_u32(r);if(c->tape_audio_phase>=c->hz||c->tape_audio_count>48000)return 0;
    for(unsigned i=0;i<c->tape_audio_count;++i)c->tape_audio[i]=(int16_t)rc_sr_u16(r);c->source=(uint8_t)source;c->physical=(uint8_t)physical;
    if(!rc_sr_ok(r))return 0;
    rc_media *tape=c->synthetic?c->synthetic:source<3&&physical?c->media[source]:NULL;char error[128];if(tape&&!rc_datasette_insert(&c->tape,tape,error,sizeof error))return 0;
    if(!rc_sr_chunk(r,"DSET",0)||!rc_datasette_load(&c->tape,r)||!rc_sr_chunk(r,"TRAP",0)||!rc_traps_load(&c->traps,r))return 0;
    if(c->media[2]&&!rc_c1541_mount(&c->drive,c->media[2],error,sizeof error))return 0;
    if(!rc_sr_chunk(r,"1541",0)||!rc_c1541_load(&c->drive,r)||!rc_sr_chunk(r,"IEC ",0)||!rc_iec_load(&c->iec,r))return 0;
    if(!rc_sr_chunk(r,"FAST",0)||!rc_fastload_load(&c->fastload,r))return 0;
    if(!rc_sr_chunk(r,"DSND",0)||!rc_drivesnd_load(&c->drive_sound,r))return 0;
    return 1;
}
