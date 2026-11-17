#include "c1541.h"
#include <stdlib.h>
#include <string.h>

static unsigned current_track(const rc_c1541 *d){return d->g64.image?d->halftrack:(d->halftrack+1u)&~1u;}
static void serial_lines(rc_c1541 *d){
    /* Input-configured VIA pins are pulled high at the inverter input;
     * the 7406 remains connected and therefore pulls its IEC line low. */
    const uint8_t out=rc_via6522_port_output(&d->via[0],1);
    const unsigned atn=!(d->iec_in&RC_IEC_ATN);
    unsigned pins=0xffu&~(1u|4u|0x80u|0x60u);
    if(!(d->iec_in&RC_IEC_DATA))pins|=1;
    if(!(d->iec_in&RC_IEC_CLK))pins|=4;
    if(atn)pins|=0x80u;
    pins|=(unsigned)(d->device-8u)<<5;
    rc_via6522_port_input(&d->via[0],1,(uint8_t)pins);
    rc_via6522_input(&d->via[0],RC_VIA_CA1,(int)atn);
    unsigned released=RC_IEC_ALL;
    if((out&2u)||(atn!=((out>>4)&1u)))released&=~RC_IEC_DATA;
    if(out&8u)released&=~RC_IEC_CLK;
    d->iec_out=(uint8_t)released;
}
static void mechanism_lines(rc_c1541 *d){
    const uint8_t out=rc_via6522_port_output(&d->via[1],1);
    const unsigned phase=out&3u;
    if(phase!=d->phase){
        const unsigned before=d->g64.image?d->rotation_track:current_track(d);
        const unsigned delta=(phase-d->phase)&3u;
        /* At a stop the coils keep cycling while the head cannot move.
         * Re-engage at the stop's phase parity before moving inwards:
         * whole-track centres have even phases (Inside Commodore DOS). */
        if(delta==1){if(d->halftrack<85){if(d->halftrack!=2||(phase&1u))++d->halftrack;}else ++d->bumps;++d->steps;}
        else if(delta==3){if(d->halftrack>2){if(d->halftrack!=85||!(phase&1u))--d->halftrack;}else ++d->bumps;++d->steps;}
        d->phase=(uint8_t)phase;
        const unsigned after=current_track(d);
        if(before!=after&&before<86&&after<86&&d->track[before].bits&&d->track[after].bits)
            d->track[after].position=(uint32_t)((uint64_t)d->track[before].position*d->track[after].bits/d->track[before].bits);
        if(d->g64.image&&after<86&&d->track[after].bits)d->rotation_track=(uint8_t)after;
    }
    d->motor=(out>>2)&1u;d->led=(out>>3)&1u;
    const unsigned writing=rc_via6522_control_output(&d->via[1],RC_VIA_CB2)==0;
    if(d->writing&&!writing)rc_c1541_flush(d);
    if(d->writing!=writing){d->separator_ticks=0;d->separator_bit=0;}
    if(!d->writing&&writing){d->bit_count=0;d->write_shift=rc_via6522_port_output(&d->via[1],0);}
    d->writing=(uint8_t)writing;
    unsigned pins=0xffu;
    if(!d->image||!d->image->writable)pins&=~16u;
    if(d->ones>=10&&!writing)pins&=~128u;
    rc_via6522_port_input(&d->via[1],1,(uint8_t)pins);
    rc_via6522_port_input(&d->via[1],0,d->read_latch);
}
void rc_c1541_device(rc_c1541 *d,unsigned device){if(device>=8&&device<=11)d->device=(uint8_t)device;serial_lines(d);}
void rc_c1541_iec_input(rc_c1541 *d,unsigned lines){
    const unsigned reset=(d->iec_in&RC_IEC_RESET)&&!(lines&RC_IEC_RESET);
    /* Unchanged lines change nothing: the port side is resolved after every VIA access and tick. */
    if(!reset&&d->iec_in==(uint8_t)(lines&RC_IEC_ALL))return;
    d->iec_in=(uint8_t)(lines&RC_IEC_ALL);if(reset)rc_c1541_reset(d);serial_lines(d);
}
unsigned rc_c1541_iec_output(const rc_c1541 *d){return d->enabled?d->iec_out:RC_IEC_ALL;}
uint8_t rc_c1541_read(rc_c1541 *d,uint16_t a){
    if(a>=0xc000)return d->rom[a&0x3fffu];
    if((a&0x1c00u)==0x1800u)return rc_via6522_read(&d->via[0],a&15u);
    if((a&0x1c00u)==0x1c00u)return rc_via6522_read(&d->via[1],a&15u);
    if(!(a&0x1800u))return d->ram[a&0x7ffu];
    return (uint8_t)(a>>8);
}
void rc_c1541_write(rc_c1541 *d,uint16_t a,uint8_t v){
    if(a>=0x8000)return;
    if((a&0x1c00u)==0x1800u){rc_via6522_write(&d->via[0],a&15u,v);serial_lines(d);}
    else if((a&0x1c00u)==0x1c00u){rc_via6522_write(&d->via[1],a&15u,v);mechanism_lines(d);}
    else if(!(a&0x1800u))d->ram[a&0x7ffu]=v;
}
static void byte_ready(rc_c1541 *d){
    d->read_latch=d->read_shift;
    rc_via6522_port_input(&d->via[1],0,d->read_latch);
    /* CA2/SOE gates the active-low byte-ready pulse to CA1 and CPU SO. */
    if(rc_via6522_control_output(&d->via[1],RC_VIA_CA2)!=0){
        d->byte_ready=1;rc_via6522_input(&d->via[1],RC_VIA_CA1,0);rc_mos6510_input(&d->cpu,RC_CPU_SO,1);
    }
}
static int blank_track(rc_gcr_track *raw,unsigned zone){
    const unsigned bytes=3200000u/(rc_gcr_bit_ticks(zone)*8u);uint8_t data[8000],speeds[2000];memset(data,0,bytes);memset(speeds,(int)(zone*0x55u),(bytes+3u)/4u);
    if(!rc_gcr_track_open(raw,data,bytes,bytes*8u,zone))return 0;
    if(!rc_gcr_track_speeds(raw,speeds)){rc_gcr_track_close(raw);return 0;}raw->image->writable=1;return 1;
}
static int media_bit(rc_c1541 *d){
    const unsigned t=current_track(d);rc_gcr_track *raw=t<86?&d->track[t]:NULL;
    if(raw&&raw->bits)return rc_gcr_track_read(raw);
    if(d->g64.image&&d->track[d->rotation_track].bits)rc_gcr_track_read(&d->track[d->rotation_track]);
    return 0;
}
static void read_bit(rc_c1541 *d,unsigned bit){
    d->read_shift=(uint8_t)((d->read_shift<<1)|bit);
    d->ones=(uint8_t)(bit?(d->ones<10?d->ones+1:10):0);
    if(d->ones>=10)d->bit_count=0;
    else if(++d->bit_count==8){d->bit_count=0;byte_ready(d);}
}
static void disk_bit(rc_c1541 *d){
    const unsigned t=current_track(d);rc_gcr_track *raw=t<86?&d->track[t]:NULL;
    int bit=0;
    if(d->writing){
        /* The parallel input is loaded at the next byte's first bit, after
         * the previous byte-ready pulse let the CPU update Port A. */
        if(!d->bit_count)d->write_shift=rc_via6522_port_output(&d->via[1],0);
        bit=d->write_shift>>7;d->write_shift<<=1;
        if(raw&&d->image&&d->image->writable){
            const unsigned zone=(rc_via6522_port_output(&d->via[1],1)>>5)&3u;
            if(!raw->bits&&d->g64.image){
                if(!blank_track(raw,zone))snprintf(d->image->error,sizeof d->image->error,"Cannot allocate G64 track");
                else {const rc_gcr_track *previous=&d->track[d->rotation_track];if(previous->bits)raw->position=(uint32_t)((uint64_t)previous->position*raw->bits/previous->bits);d->rotation_track=(uint8_t)t;}
            }
            if(raw->bits){rc_gcr_track_write_zone(raw,zone);if(rc_gcr_track_write(raw,bit))d->dirty_track[t]=1;}
        }
        else if(raw&&raw->bits)rc_gcr_track_read(raw);
        else if(d->g64.image&&d->track[d->rotation_track].bits)rc_gcr_track_read(&d->track[d->rotation_track]);
        if(++d->bit_count==8){d->bit_count=0;byte_ready(d);}
        d->ones=0;
    }else read_bit(d,(unsigned)media_bit(d));
}
static void g64_read(rc_c1541 *d,unsigned density){
    const unsigned period=rc_gcr_bit_ticks(density);
    /* Recorded transitions and the density-selected data separator have
     * separate clocks. A transition recentres the read cell; its centre
     * clocks the latched bit into the shift register. Work in 16 MHz ticks
     * so speed-map changes never round a 3.25 us cell to whole CPU cycles. */
    for(unsigned tick=0;tick<16;++tick){
        const unsigned t=current_track(d);const rc_gcr_track *raw=t<86?&d->track[t]:NULL;
        if(!raw||!raw->bits)raw=&d->track[d->rotation_track];
        const unsigned recorded=raw->bits?rc_gcr_bit_ticks(rc_gcr_track_zone(raw)):period;
        ++d->bit_ticks;++d->separator_ticks;
        if(d->bit_ticks>=recorded){d->bit_ticks-=recorded;if(media_bit(d)){d->separator_bit=1;d->separator_ticks=period/2;}}
        if(d->separator_ticks>=period){d->separator_ticks-=period;read_bit(d,d->separator_bit);d->separator_bit=0;}
    }
}
void rc_c1541_tick(rc_c1541 *d){
    if(!d->enabled)return;++d->cycles;
    if(d->byte_ready){d->byte_ready=0;rc_via6522_input(&d->via[1],RC_VIA_CA1,1);rc_mos6510_input(&d->cpu,RC_CPU_SO,0);}
    rc_via6522_tick(&d->via[0]);rc_via6522_tick(&d->via[1]);
    if(d->motor){
        const unsigned zone=(rc_via6522_port_output(&d->via[1],1)>>5)&3u;
        if(d->g64.image&&!d->writing)g64_read(d,zone);
        else {d->bit_ticks+=16;while(d->bit_ticks>=rc_gcr_bit_ticks(zone)){d->bit_ticks-=rc_gcr_bit_ticks(zone);disk_bit(d);}}
    }
    mechanism_lines(d);serial_lines(d);
    rc_mos6510_input(&d->cpu,RC_CPU_IRQ,d->via[0].irq||d->via[1].irq);
    const rc_cpu_cycle p=d->cpu.pins;
    uint8_t value=p.data;if(p.write)rc_c1541_write(d,p.address,p.data);else value=rc_c1541_read(d,p.address);
    rc_mos6510_clock(&d->cpu,value,1);
}
void rc_c1541_reset(rc_c1541 *d){
    rc_via6522_reset(&d->via[0]);rc_via6522_reset(&d->via[1]);rc_mos6510_reset(&d->cpu);
    /* RESET changes VIA pins, not the mechanism's last coil position. */
    d->motor=d->led=d->writing=d->byte_ready=0;d->bit_count=d->ones=d->separator_bit=0;d->bit_ticks=d->separator_ticks=0;
    rc_mos6510_input(&d->cpu,RC_CPU_SO,0);serial_lines(d);mechanism_lines(d);
}
void rc_c1541_init(rc_c1541 *d,const uint8_t rom[16384],unsigned device){
    memset(d,0,sizeof *d);d->enabled=rom!=NULL;if(rom)memcpy(d->rom,rom,sizeof d->rom);
    d->halftrack=36;d->device=(uint8_t)(device>=8&&device<=11?device:8);d->iec_in=d->iec_out=RC_IEC_ALL;
    rc_mos6510_init(&d->cpu,NULL,RC_MOS6502);rc_via6522_init(&d->via[0],NULL);rc_via6522_init(&d->via[1],NULL);rc_c1541_reset(d);
}
static uint8_t raw_byte(const rc_gcr_track *raw,uint32_t start){uint8_t result=0;for(unsigned b=0;b<8;++b){const uint32_t p=(start+b)%raw->bits;result=(uint8_t)((result<<1)|(raw->image->data[p/8]>>(7u-p%8)&1u));}return result;}
int rc_c1541_flush(rc_c1541 *d){
    if(!d->image)return 1;
    if(d->g64.image){
        for(unsigned h=2;h<86;++h)if(d->dirty_track[h]){
            const rc_gcr_track *raw=&d->track[h];
            if(!rc_g64_write(&d->g64,h-2,raw->image->data,(unsigned)raw->image->size,raw->speeds?raw->speeds->data:NULL,raw->zone))return rc_image_error(d->image->error,sizeof d->image->error,"Cannot store G64 track");
            d->dirty_track[h]=0;
        }return 1;
    }
    for(unsigned t=1;t<=d->disk.tracks;++t)if(d->dirty_track[t*2]){
        rc_gcr_track *raw=&d->track[t*2];unsigned ones=0,header_sector=UINT32_MAX;uint32_t last_header=0;
        /* Search bit-aligned sync marks; a write may start at any bit. */
        for(uint32_t p=0;p<raw->bits+3000u;++p){const uint32_t at=p%raw->bits;const unsigned bit=raw->image->data[at/8]>>(7u-at%8)&1u;
            if(bit){if(ones<255)++ones;continue;}
            if(ones>=10){uint8_t encoded[325],decoded[4];for(unsigned i=0;i<5;++i)encoded[i]=raw_byte(raw,p+i*8u);
                if(rc_gcr_decode5(encoded,decoded)){
                    if(decoded[0]==8){for(unsigned i=5;i<10;++i)encoded[i]=raw_byte(raw,p+i*8u);uint8_t ht,hs,id[2];if(rc_gcr_header_read(encoded,&ht,&hs,id)&&ht==t&&hs<rc_d64_track_sectors(t)){header_sector=hs;last_header=p;}}
                    else if(decoded[0]==7&&header_sector!=UINT32_MAX&&p-last_header<800u){for(unsigned i=5;i<325;++i)encoded[i]=raw_byte(raw,p+i*8u);uint8_t data[256];if(rc_gcr_data_read(encoded,data))rc_d64_write_sector(&d->disk,t,header_sector,data);header_sector=UINT32_MAX;}
                }
            }ones=0;
        }d->dirty_track[t*2]=0;
    }return 1;
}
static void unmount(rc_c1541 *d){rc_c1541_flush(d);for(unsigned t=2;t<86;++t)rc_gcr_track_close(&d->track[t]);memset(&d->disk,0,sizeof d->disk);memset(&d->g64,0,sizeof d->g64);d->image=NULL;rc_media_close(d->owned_image);d->owned_image=NULL;memset(d->dirty_track,0,sizeof d->dirty_track);}
int rc_c1541_mount(rc_c1541 *d,rc_media *image,char *error,size_t cap){
    if(!rc_c1541_flush(d))return rc_image_error(error,cap,"Cannot flush mounted disk");
    if(!image){unmount(d);return 1;}
    rc_d64 disk={0};rc_g64 g64={0};rc_gcr_track tracks[86]={0};
    if(rc_media_extension_in(image->path,"g64")||(image->size>=8&&!memcmp(image->data,"GCR-1541",8))){
        if(!rc_g64_open(&g64,image,error,cap))return 0;
        for(unsigned i=0;i<g64.tracks;++i)if(g64.offset[i]){
            rc_gcr_track *raw=&tracks[i+2];const unsigned bytes=g64.length[i],zone=rc_g64_speed(&g64,i,0);
            if(!rc_gcr_track_open(raw,image->data+g64.offset[i]+2,bytes,bytes*8u,zone))goto no_memory;
            uint8_t speeds[16384];for(unsigned n=0;n<(bytes+3u)/4u;++n)speeds[n]=(uint8_t)(zone*0x55u);
            if(g64.speed[i]>3)memcpy(speeds,image->data+g64.speed[i],(bytes+3u)/4u);
            if(!rc_gcr_track_speeds(raw,speeds))goto no_memory;raw->image->writable=1;
        }
    }else{
    if(!rc_d64_open(&disk,image->data,image->size,error,cap))return 0;rc_media_close(disk.image);disk.image=image;
    const uint8_t *bam=rc_d64_sector(&disk,18,0);const uint8_t id[2]={bam[162],bam[163]};
    for(unsigned t=1;t<=disk.tracks;++t){
        const unsigned zone=rc_gcr_zone(t),bytes=3200000u/(rc_gcr_bit_ticks(zone)*8u);uint8_t raw[8000];memset(raw,0x55,bytes);size_t at=0;
        for(unsigned s=0;s<rc_d64_track_sectors(t);++s){
            uint8_t header[10],data[325];rc_gcr_header((uint8_t)t,(uint8_t)s,id,header);rc_gcr_data(rc_d64_sector(&disk,t,s),data);
            const unsigned err=rc_d64_error(&disk,t,s);
            /* Preserve legal GCR codes while changing the documented
             * descriptor, checksum or disk ID, so DOS sees that error. */
            if(err==2||err==9||err==11){uint8_t h[8];rc_gcr_decode5(header,h);rc_gcr_decode5(header+5,h+4);
                if(err==2)h[0]=0;
                if(err==9)h[1]^=0xffu;
                if(err==11){h[4]^=0xffu;h[1]^=0xffu;}
                rc_gcr_encode4(h,header);rc_gcr_encode4(h+4,header+5);
            }
            if(err==4){uint8_t group[4];rc_gcr_decode5(data,group);group[0]=0;rc_gcr_encode4(group,data);}
            if(err==5){uint8_t group[4];rc_gcr_decode5(data+320,group);group[1]^=0xffu;rc_gcr_encode4(group,data+320);}
            memset(raw+at,err==3?0x55:0xff,5);at+=5;memcpy(raw+at,header,10);at+=10;at+=9;
            memset(raw+at,err==3?0x55:0xff,5);at+=5;memcpy(raw+at,data,325);at+=325;at+=8;
        }
        if(!rc_gcr_track_open(&tracks[t*2],raw,bytes,bytes*8u,zone))goto no_memory;tracks[t*2].image->writable=1;
    }}
    unmount(d);d->image=image;d->disk=disk;d->g64=g64;memcpy(d->track,tracks,sizeof tracks);
    d->rotation_track=0;for(unsigned h=2;h<86;++h)if(d->track[h].bits){d->rotation_track=(uint8_t)h;if(h>=d->halftrack)break;}
    mechanism_lines(d);return 1;
no_memory:
    for(unsigned h=2;h<86;++h)rc_gcr_track_close(&tracks[h]);return rc_image_error(error,cap,"Out of memory");
}
rc_media *rc_c1541_image(const rc_c1541 *d){return d->image;}
void rc_c1541_close(rc_c1541 *d){unmount(d);}
rc_c1541_status rc_c1541_get_status(const rc_c1541 *d){return (rc_c1541_status){d->device,d->halftrack,d->motor,d->led,d->writing,d->steps,d->bumps,d->cycles};}
void rc_c1541_save(const rc_c1541 *d,rc_state_writer *w){
    rc_sw_u32(w,2);rc_mos6510_save(&d->cpu,w);rc_via6522_save(&d->via[0],w);rc_via6522_save(&d->via[1],w);rc_sw_packed(w,d->ram,sizeof d->ram);
    rc_sw_u64(w,d->cycles);rc_sw_u64(w,d->steps);rc_sw_u64(w,d->bumps);rc_sw_u32(w,d->bit_ticks);
    rc_sw_u32(w,d->separator_ticks);
#define B(n) rc_sw_u8(w,d->n)
    B(bit_count);B(read_shift);B(write_shift);B(read_latch);B(ones);B(halftrack);B(phase);B(motor);B(led);B(writing);B(byte_ready);B(iec_in);B(iec_out);B(device);B(enabled);B(rotation_track);B(separator_bit);
#undef B
    rc_sw_bytes(w,d->dirty_track,sizeof d->dirty_track);rc_sw_u8(w,d->image!=NULL);if(d->image)rc_image_save(d->image,w);
    if(d->image)for(unsigned h=2;h<86;++h){rc_sw_u8(w,d->track[h].image!=NULL);if(d->track[h].image)rc_gcr_track_save(&d->track[h],w);}
}
int rc_c1541_load(rc_c1541 *d,rc_state_reader *r){
    const uint32_t version=rc_sr_u32(r);if(version<1||version>2)return 0;rc_c1541 *n=calloc(1,sizeof *n);if(!n)return 0;rc_c1541_init(n,d->enabled?d->rom:NULL,d->device);
    int ok=rc_mos6510_load(&n->cpu,r)&&rc_via6522_load(&n->via[0],r)&&rc_via6522_load(&n->via[1],r);rc_sr_packed(r,n->ram,sizeof n->ram);
    n->cycles=rc_sr_u64(r);n->steps=rc_sr_u64(r);n->bumps=rc_sr_u64(r);n->bit_ticks=rc_sr_u32(r);
    if(version==2)n->separator_ticks=rc_sr_u32(r);
#define B(v) n->v=rc_sr_u8(r)
    B(bit_count);B(read_shift);B(write_shift);B(read_latch);B(ones);B(halftrack);B(phase);B(motor);B(led);B(writing);B(byte_ready);B(iec_in);B(iec_out);B(device);B(enabled);
    if(version==2){B(rotation_track);B(separator_bit);}
#undef B
    uint8_t dirty[86]={0};if(version==1){uint8_t old[43];rc_sr_bytes(r,old,sizeof old);for(unsigned t=1;t<=42;++t)dirty[t*2]=old[t];}else rc_sr_bytes(r,dirty,sizeof dirty);
    const unsigned present=rc_sr_u8(r);rc_media *image=present==1?rc_image_load(r):NULL;ok=ok&&present<=1&&(!present||image!=NULL);
    const uint8_t rotation=n->rotation_track;
    if(image){char error[128];ok=ok&&rc_c1541_mount(n,image,error,sizeof error);n->owned_image=image;
        if(version==1){if(n->g64.image)ok=0;for(unsigned t=1;ok&&t<=n->disk.tracks;++t)ok=rc_gcr_track_load(&n->track[t*2],r);}
        else for(unsigned h=2;ok&&h<86;++h){const unsigned exists=rc_sr_u8(r);if(exists>1)ok=0;else if(exists)ok=rc_gcr_track_load(&n->track[h],r);else rc_gcr_track_close(&n->track[h]);}
    }
    n->rotation_track=rotation;
    memcpy(n->dirty_track,dirty,sizeof dirty);
    /* Older board resets could leave the coils half a step out of phase.
     * Keep the same D64 track while restoring the physical phase parity. */
    if(version==1&&n->halftrack>2&&((n->halftrack^n->phase)&1u))n->halftrack=(uint8_t)(n->halftrack+(n->phase&1u?-1:1));
    ok=ok&&rc_sr_ok(r)&&n->halftrack>=2&&n->halftrack<=85&&n->rotation_track<86&&n->bit_count<8&&n->separator_ticks<64&&n->separator_bit<=1&&n->device>=8&&n->device<=11;
    if(ok){rc_c1541_flush(d);rc_media *borrowed=d->image==d->owned_image?NULL:d->image;
        if(borrowed&&n->image){uint8_t *bytes=n->image->data;size_t size=n->image->size;n->image->data=borrowed->data;n->image->size=borrowed->size;borrowed->data=bytes;borrowed->size=size;borrowed->dirty=n->image->dirty;rc_media_close(n->owned_image);n->owned_image=NULL;n->image=borrowed;if(n->g64.image)n->g64.image=borrowed;else n->disk.image=borrowed;}
        unmount(d);*d=*n;
    }else rc_c1541_close(n);free(n);return ok;
}
