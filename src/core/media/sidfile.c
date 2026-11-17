/* Original reader and 6510 driver from the HVSC format and MOS instruction set. */
#include "sidfile.h"
#include <stdio.h>
#include <string.h>

static int fail(char *error,size_t cap,const char *why){if(cap)snprintf(error,cap,"%s",why);return 0;}
static uint16_t be16(const uint8_t *p){return (uint16_t)((uint16_t)p[0]<<8|p[1]);}
static uint32_t be32(const uint8_t *p){return (uint32_t)p[0]<<24|(uint32_t)p[1]<<16|(uint32_t)p[2]<<8|p[3];}
static void text(char out[33],const uint8_t *in){memcpy(out,in,32);out[32]=0;}
static int extra_sid(unsigned v){return !(v&1u)&&((v>=0x42&&v<=0x7e)||v>=0xe0);}
static int driver_page(unsigned page){return (page>=4&&page<0xa0)||(page>=0xc0&&page<0xd0);}
static int overlaps(unsigned a,unsigned n,unsigned b,unsigned m){return a<b+m&&b<a+n;}
int rc_sidfile_parse(rc_sidfile *out,const uint8_t *d,size_t size,char *error,size_t cap){
    rc_sidfile s={0};
    if(!d||size<0x76||(memcmp(d,"PSID",4)&&memcmp(d,"RSID",4)))return fail(error,cap,"Not a PSID or RSID file");
    s.rsid=(uint8_t)(d[0]=='R');s.version=be16(d+4);
    if(s.version<1||s.version>4||(s.rsid&&s.version<2))return fail(error,cap,"Unsupported SID version (PSID 1-4 / RSID 2-4 required)");
    const size_t header=s.version==1?0x76u:0x7cu;
    if(size<header||be16(d+6)!=header)return fail(error,cap,"Invalid SID header or data offset");
    s.load=be16(d+8);s.init=be16(d+10);s.play=be16(d+12);s.songs=be16(d+14);s.start_song=be16(d+16);s.speed=be32(d+18);
    if(!s.songs||s.songs>256||!s.start_song||s.start_song>s.songs)return fail(error,cap,"Invalid SID song count or starting song");
    text(s.title,d+22);text(s.author,d+54);text(s.released,d+86);
    if(s.version>=2){
        s.flags=be16(d+118);s.clock=(uint8_t)((s.flags>>2)&3u);s.model=(uint8_t)((s.flags>>4)&3u);
        s.start_page=d[120];s.pages=d[121];s.basic=(uint8_t)(s.rsid&&(s.flags&2u));
        if(s.flags&1u)return fail(error,cap,"SID requires an external Compute! MUS player");
        if((s.version==2&&(d[122]||d[123]))||(s.version==3&&d[123]))return fail(error,cap,"Invalid reserved SID address field");
        if((s.version>=3&&extra_sid(d[122]))||(s.version>=4&&extra_sid(d[123])))return fail(error,cap,"Multiple SID chips are not supported");
        if((!s.start_page||s.start_page==255)&&s.pages)return fail(error,cap,"Invalid SID relocation page count");
    }
    if(s.rsid&&(s.load||s.play||s.speed||(s.basic&&s.init)))return fail(error,cap,"Invalid RSID reserved fields");
    size_t offset=header;
    if(!s.load){if(size-offset<2)return fail(error,cap,"Missing SID load address");s.load=(uint16_t)(d[offset]|(uint16_t)d[offset+1]<<8);offset+=2;}
    if(size-offset==0||size-offset>65536u-s.load)return fail(error,cap,"SID payload is empty or exceeds C64 memory");
    s.payload=d+offset;s.length=(uint32_t)(size-offset);
    if(!s.init&&!s.basic)s.init=s.load;
    if(!s.basic&&(s.init<s.load||(uint32_t)s.init>=s.load+s.length))return fail(error,cap,"SID init address is outside its payload");
    if(s.rsid&&(s.load<0x07e8||(!s.basic&&(s.init<0x07e8||(s.init>=0xa000&&s.init<0xc000)||s.init>=0xd000))))return fail(error,cap,"RSID load or init address is outside permitted RAM");
    if(s.start_page&&s.start_page!=255){
        if(!s.pages||(unsigned)s.start_page+s.pages>256||overlaps((unsigned)s.start_page<<8,(unsigned)s.pages<<8,s.load,s.length))return fail(error,cap,"SID relocation range overlaps payload or wraps memory");
        for(unsigned page=s.start_page;page<(unsigned)s.start_page+s.pages;++page)if(!driver_page(page))return fail(error,cap,"SID relocation range includes reserved memory");
    }
    *out=s;return 1;
}
int rc_sidfile_cia(const rc_sidfile *s,unsigned song){unsigned bit=song;if(s->version==1||(s->flags&2u))bit&=31u;else if(bit>31)bit=31;return (int)((s->speed>>bit)&1u);}
uint8_t rc_sidfile_bank(uint16_t a){return a<0xa000?0x37:a<0xd000?0x36:a>=0xe000?0x35:0x34;}

/* Tiny emitter keeps every relocation explicit. No assembler or foreign driver. */
static void emit(rc_sid_driver *d,unsigned byte){d->bytes[d->length++]=(uint8_t)byte;}
static void imm(rc_sid_driver *d,unsigned op,unsigned value){emit(d,op);emit(d,value);}
static void absop(rc_sid_driver *d,unsigned op,unsigned addr){emit(d,op);emit(d,addr);emit(d,addr>>8);}
static unsigned here(const rc_sid_driver *d){return d->address+d->length;}
static void bank(rc_sid_driver *d,unsigned value){imm(d,0xa9,value);imm(d,0x85,1);}
int rc_sidfile_driver(const rc_sidfile *s,unsigned song,rc_sid_driver *out,char *error,size_t cap){
    if(s->rsid||song>=s->songs)return fail(error,cap,"Invalid PSID driver request");
    unsigned page=0;
    if(s->start_page==255)return fail(error,cap,"PSID reserves no memory for its playback driver");
    if(s->start_page)page=s->start_page;
    else for(unsigned p=4;p<0xd0;++p)if(driver_page(p)&&!overlaps(p<<8,256,s->load,s->length)){page=p;break;}
    if(!page)return fail(error,cap,"PSID has no free RAM page for its playback driver");
    rc_sid_driver d={0};d.address=d.entry=(uint16_t)(page<<8);
    emit(&d,0x78);emit(&d,0xd8); /* SEI / CLD */
    bank(&d,rc_sidfile_bank(s->init));imm(&d,0xa9,song);imm(&d,0xa2,0);imm(&d,0xa0,0);absop(&d,0x20,s->init);
    d.initialized=(uint16_t)here(&d);
    /* Idle with the original KERNAL vector visible; the per-call bank can
     * expose a playback routine anywhere without replacing payload vectors. */
    if(s->play)bank(&d,overlaps(s->load,s->length,0x0314,2)?0x35:0x37);
    emit(&d,0x58);d.idle=(uint16_t)here(&d);absop(&d,0x4c,d.idle);
    d.irq=(uint16_t)here(&d);emit(&d,0x48);emit(&d,0x8a);emit(&d,0x48);emit(&d,0x98);emit(&d,0x48); /* save A X Y */
    if(!s->play){
        absop(&d,0x6c,0x0314); /* a banked-out KERNAL's standard IRQ dispatch */
        d.indirect_irq=(uint16_t)here(&d);
    }else{
        d.indirect_irq=(uint16_t)here(&d);
        imm(&d,0xa5,1);emit(&d,0x48);bank(&d,0x35);
        imm(&d,0xa9,0x0f);absop(&d,0x8d,0xd019);absop(&d,0xad,0xdc0d);
        bank(&d,rc_sidfile_bank(s->play));absop(&d,0x20,s->play);
        emit(&d,0x68);imm(&d,0x85,1); /* restore bank */
    }
    emit(&d,0x68);emit(&d,0xa8);emit(&d,0x68);emit(&d,0xaa);emit(&d,0x68);emit(&d,0x40);
    d.nmi=(uint16_t)here(&d);emit(&d,0x40);*out=d;return 1;
}
