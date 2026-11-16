/* SID launch in the real board: original firmware boot, no ROM alterations. */
#include "c64_sid.h"
#include "c64.h"
#include <stdio.h>
#include <string.h>

enum { SID_BOOT=1,SID_INIT,SID_RUNNING };
static void word(uint8_t *ram,unsigned address,unsigned value){ram[address]=(uint8_t)value;ram[address+1]=(uint8_t)(value>>8);}
static int payload_at(const rc_sidfile *f,unsigned address,unsigned count){return f->load<address+count&&address<f->load+f->length;}
static void io(rc_c64 *c,unsigned address,unsigned value){rc_bus_write(&c->bus,(uint16_t)address,(uint8_t)value);}
static int ready(const rc_c64 *c){
    /* Inspect the original screen's READY. prompt, never a guessed ROM PC. */
    static const uint8_t prompt[]={18,5,1,4,25,46};
    const unsigned bank=(rc_cia6526_port_output(&c->cia[1],0)^3u)&3u;
    const unsigned screen=(bank<<14)|((c->vic.reg[24]&240u)<<6);
    for(unsigned row=0;row<25;++row){unsigned col=0;while(col<34&&c->bus.ram[(screen+row*40+col)&65535u]==32)++col;
        if(col<=34&&!memcmp(c->bus.ram+((screen+row*40+col)&65535u),prompt,sizeof prompt))return 1;}
    return 0;
}
static void environment(rc_c64 *c,const rc_sidfile *f,unsigned song){
    io(c,0,0x2f);io(c,1,0x37);
    for(unsigned chip=0;chip<2;++chip){const unsigned base=0xdc00+chip*256;
        io(c,base+14,0);io(c,base+15,0);io(c,base+13,0x7f);
        io(c,base+4,255);io(c,base+5,255);io(c,base+6,255);io(c,base+7,255);
        (void)rc_bus_read(&c->bus,(uint16_t)(base+13),1);
    }
    const unsigned timer=c->model>=2?0x4295:0x4025;
    io(c,0xdc04,timer);io(c,0xdc05,timer>>8);io(c,0xdc0e,0x11);
    const int cia=f->rsid||rc_sidfile_cia(f,song);
    io(c,0xdc0d,cia?0x81:0x7f);
    io(c,0xd011,f->rsid?c->vic.reg[17]|0x80u:c->vic.reg[17]&0x7fu);
    io(c,0xd012,f->rsid?0x37:0);io(c,0xd019,0x0f);io(c,0xd01a,cia?0:1);
    c->bus.ram[0x02a6]=(uint8_t)(c->model<2);
}
static void launch(rc_c64 *c){
    rc_sid_session *s=&c->sid_session;const rc_sidfile *f=&s->file;
    environment(c,f,s->song);
    /* Physical RAM loading must also reach RAM beneath I/O and ROM. */
    memcpy(c->bus.ram+f->load,f->payload,f->length);
    rc_sid_reset(&c->sid);
    s->elapsed_cycles=0;s->phase=SID_RUNNING;
    if(!f->rsid){
        rc_sid_driver d;char error[128];
        if(!rc_sidfile_driver(f,s->song,&d,error,sizeof error)){s->active=0;return;} /* prevalidated on insertion */
        memcpy(c->bus.ram+d.address,d.bytes,d.length);
        /* A play=0 init can bank out KERNAL. Supply a normal IRQ shim only
         * where the payload has not supplied its own physical vectors. */
        if(!f->play||payload_at(f,0x0314,2)){
            if(!payload_at(f,0xfffe,2))word(c->bus.ram,0xfffe,d.irq);
            if(!payload_at(f,0xfffa,2))word(c->bus.ram,0xfffa,d.nmi);
        }
        if(!payload_at(f,0x0314,2))word(c->bus.ram,0x0314,d.indirect_irq);
        c->cpu.pc=d.entry;c->cpu.p=RC_P_U|RC_P_I;c->cpu.s=0xff;c->cpu.a=c->cpu.x=c->cpu.y=0;
        c->cpu.irq_pending=c->cpu.irq_poll=c->cpu.interrupt=c->cpu.resetting=0;rc_mos6510_start(&c->cpu);
    }else if(f->basic){
        const unsigned end=f->load+f->length;
        word(c->bus.ram,0x2b,f->load);word(c->bus.ram,0x2d,end);word(c->bus.ram,0x2f,end);word(c->bus.ram,0x31,end);
        c->bus.ram[0x030c]=s->song;memcpy(c->bus.ram+0x0277,"RUN\r",4);c->bus.ram[0xc6]=4;
    }else{
        /* An ordinary JSR-equivalent on the real stack. On return, resume
         * the firmware's existing input loop with its register context. A
         * tune that never returns remains free to run its own busy loop. */
        s->return_pc=c->cpu.pins.address;s->return_s=c->cpu.s;
        s->return_a=c->cpu.a;s->return_x=c->cpu.x;s->return_y=c->cpu.y;s->return_p=c->cpu.p;
        const uint16_t ret=(uint16_t)(s->return_pc-1);
        c->bus.ram[0x100+c->cpu.s--]=(uint8_t)(ret>>8);c->bus.ram[0x100+c->cpu.s--]=(uint8_t)ret;
        c->cpu.pc=f->init;c->cpu.a=s->song;c->cpu.x=c->cpu.y=0;c->cpu.p=(uint8_t)((c->cpu.p|RC_P_U)&~RC_P_D);
        rc_mos6510_start(&c->cpu);s->phase=SID_INIT;
    }
}
int rc_c64_sid_insert(rc_c64 *c,rc_media *image,char *error,size_t cap){
    rc_sidfile file;if(!image||!rc_sidfile_parse(&file,image->data,image->size,error,cap))return 0;
    if(!file.rsid){rc_sid_driver d;if(!rc_sidfile_driver(&file,file.start_song-1u,&d,error,cap))return 0;}
    rc_sid_session session={0};session.file=file;session.song=(uint8_t)(file.start_song-1u);session.active=1;session.phase=SID_BOOT;
    c->sid_session=session;rc_c64_family.impl->reset(c,1);return 1;
}
void rc_c64_sid_eject(rc_c64 *c){memset(&c->sid_session,0,sizeof c->sid_session);}
void rc_c64_sid_reset(rc_c64 *c){rc_sid_session *s=&c->sid_session;if(s->active){s->phase=SID_BOOT;s->boot_cycles=s->elapsed_cycles=0;s->paused=0;s->return_pc=0;s->return_s=s->return_a=s->return_x=s->return_y=s->return_p=0;}}
void rc_c64_sid_tick(rc_c64 *c){
    rc_sid_session *s=&c->sid_session;if(!s->active||s->paused)return;
    if(s->phase==SID_BOOT){
        ++s->boot_cycles;
        /* The first keyboard wait after READY has reached KERNAL and has
         * interrupts enabled. Avoid interrupting BASIC's prompt printing. */
        if(s->boot_cycles>c->hz/4&&c->cpu.pins.sync&&c->cpu.pins.address>=0xe000&&!(c->cpu.p&RC_P_I)&&
           (s->boot_cycles&1023u)==0&&ready(c))launch(c);
    }else{
        ++s->elapsed_cycles;
        if(s->phase==SID_INIT&&c->cpu.pins.sync&&c->cpu.pins.address==s->return_pc&&c->cpu.s==s->return_s){
            c->cpu.a=s->return_a;c->cpu.x=s->return_x;c->cpu.y=s->return_y;c->cpu.p=s->return_p;s->phase=SID_RUNNING;
        }
    }
}
int rc_c64_sid_paused(const rc_c64 *c){return c->sid_session.active&&c->sid_session.paused;}
int rc_c64_sid_control(rc_c64 *c,unsigned command,uint32_t value){
    rc_sid_session *s=&c->sid_session;if(!s->active)return 0;
    if(command==RC_MEDIA_SONG){if(value!=UINT32_MAX){if(value>=s->file.songs)return 0;s->song=(uint8_t)value;rc_c64_family.impl->reset(c,1);}return s->song+1;}
    if(command==RC_MEDIA_PAUSE){if(value!=UINT32_MAX){if(value>1)return 0;s->paused=(uint8_t)value;}return s->paused+1;}
    return 0;
}
void rc_c64_sid_save(const rc_c64 *c,rc_state_writer *w){
    const rc_sid_session *s=&c->sid_session;rc_sw_begin(w,"SIDP",0);rc_sw_u32(w,1);rc_sw_u8(w,s->active);
    if(s->active){rc_sw_u8(w,s->phase);rc_sw_u8(w,s->song);rc_sw_u8(w,s->paused);rc_sw_u64(w,s->elapsed_cycles);rc_sw_u64(w,s->boot_cycles);
        rc_sw_u16(w,s->return_pc);rc_sw_u8(w,s->return_s);rc_sw_u8(w,s->return_a);rc_sw_u8(w,s->return_x);rc_sw_u8(w,s->return_y);rc_sw_u8(w,s->return_p);}
    rc_sw_end(w);
}
int rc_c64_sid_load(rc_c64 *c,rc_state_reader *r){
    rc_sid_session s={0};
    if(!rc_sr_chunk(r,"SIDP",0)){rc_c64_sid_eject(c);return c->media[4]==NULL;}
    if(rc_sr_u32(r)!=1)return 0;s.active=rc_sr_u8(r);if(s.active>1)return 0;
    if(s.active){char error[128];if(!c->media[4]||!rc_sidfile_parse(&s.file,c->media[4]->data,c->media[4]->size,error,sizeof error))return 0;
        s.phase=rc_sr_u8(r);s.song=rc_sr_u8(r);s.paused=rc_sr_u8(r);s.elapsed_cycles=rc_sr_u64(r);s.boot_cycles=rc_sr_u64(r);
        s.return_pc=rc_sr_u16(r);s.return_s=rc_sr_u8(r);s.return_a=rc_sr_u8(r);s.return_x=rc_sr_u8(r);s.return_y=rc_sr_u8(r);s.return_p=rc_sr_u8(r);
        if(s.phase<SID_BOOT||s.phase>SID_RUNNING||s.song>=s.file.songs||s.paused>1||(!s.file.rsid&&s.phase==SID_INIT))return 0;
    }else if(c->media[4])return 0;
    if(!rc_sr_ok(r))return 0;c->sid_session=s;return 1;
}
