/* Board wiring from Commodore's schematics, PRG and Mapping the C64. */
#include "c64.h"
#include "c64_media.h"
#include "c64_expansion.h"
#include "c64_layout.h"
#include "../palette.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static const char *const ROMS[]={"kernal","basic","chargen",NULL};
static const rc_model_desc MODELS[]={
    {"c64-pal","C64 PAL",ROMS,NULL},{"c64c-pal","C64C PAL",ROMS,NULL},
    {"c64-ntsc","C64 NTSC",ROMS,NULL},{"c64c-ntsc","C64C NTSC",ROMS,NULL}};
static const char *const SID_MODELS[]={"6581","8580",NULL};
static const char *const DRIVE_DEVICES[]={"8","9","10","11",NULL};
static const char *const DRIVE_MODELS[]={"1541","1541-ii",NULL};
static const char *const REU_SIZES[]={"off","128k","256k","512k","1m","2m","4m","8m","16m",NULL};
static const char *const GEO_SIZES[]={"off","512k","1m","2m","4m",NULL};
static const rc_option_desc OPTIONS[]={
    {.key="sid",.values=SID_MODELS,.models=RC_MODELS_ALL,.flags=RC_OPTION_RESET},
    {.key="drive_device",.values=DRIVE_DEVICES,.models=RC_MODELS_ALL,.flags=RC_OPTION_RESET},
    {.key="reu",.values=REU_SIZES,.models=RC_MODELS_ALL,.flags=RC_OPTION_RESET},
    {.key="georam",.values=GEO_SIZES,.models=RC_MODELS_ALL,.flags=RC_OPTION_RESET},
    {.key="drive_model",.values=DRIVE_MODELS,.models=RC_MODELS_ALL,.flags=RC_OPTION_RESET}};
static const rc_media_desc MEDIA[]={
    {.key="tape",.kind=RC_MEDIA_TAPE,.extensions="tap,t64",.models=RC_MODELS_ALL},
    {.key="program",.kind=RC_MEDIA_PROGRAM,.extensions="prg,p00",.models=RC_MODELS_ALL},
    {.key="disk",.kind=RC_MEDIA_FLOPPY,.extensions="d64,g64",.models=RC_MODELS_ALL},
    {.key="cartridge",.kind=RC_MEDIA_CART,.extensions="crt",.models=RC_MODELS_ALL},
    {.key="sid",.kind=RC_MEDIA_SID,.extensions="sid",.models=RC_MODELS_ALL}};
/* Matrix order: CIA1 PA bit selects the row; PB bit selects the column. */
static rc_key_desc KEYS[]={
    {"DELETE","Backspace","DEL",0,0,4,4},{"RETURN","Return","RETURN",0,0,4,4},
    {"CURSOR RIGHT","Right","RIGHT",0,0,4,4},{"F7","F7","F7",0,0,4,4},
    {"F1","F1","F1",0,0,4,4},{"F3","F3","F3",0,0,4,4},{"F5","F5","F5",0,0,4,4},
    {"CURSOR DOWN","Down","DOWN",0,0,4,4},
#define K(n,h) {n,h,n,0,0,4,4}
    K("3","3"),K("W","W"),K("A","A"),K("4","4"),K("Z","Z"),K("S","S"),K("E","E"),K("LEFT SHIFT","Left Shift"),
    K("5","5"),K("R","R"),K("D","D"),K("6","6"),K("C","C"),K("F","F"),K("T","T"),K("X","X"),
    K("7","7"),K("Y","Y"),K("G","G"),K("8","8"),K("B","B"),K("H","H"),K("U","U"),K("V","V"),
    K("9","9"),K("I","I"),K("J","J"),K("0","0"),K("M","M"),K("K","K"),K("O","O"),K("N","N"),
    K("+",NULL),K("P","P"),K("L","L"),K("-","-"),K(".","."),K(":",NULL),K("@",NULL),K(",",","),
    K("POUND",NULL),K("*",NULL),K(";",";"),K("HOME","Home"),K("RIGHT SHIFT","Right Shift"),K("=","="),K("UP ARROW",NULL),K("/","/"),
    K("1","1"),K("LEFT ARROW",NULL),K("CTRL","Tab"),K("2","2"),K("SPACE","Space"),K("COMMODORE","PageDown"),K("Q","Q"),K("RUN STOP","Escape"),K("RESTORE","PageUp")
#undef K
};
static const rc_typing_desc TYPING[]={
#define T(ch,key) {ch,key,RC_KEY_NONE}
#define A(ch,key) T(ch,key),T(ch+32,key)
    A('A',10),A('B',28),A('C',20),A('D',18),A('E',14),A('F',21),A('G',26),A('H',29),
    A('I',33),A('J',34),A('K',37),A('L',42),A('M',36),A('N',39),A('O',38),A('P',41),
    A('Q',62),A('R',17),A('S',13),A('T',22),A('U',30),A('V',31),A('W',9),A('X',23),A('Y',25),A('Z',12),
    T('0',35),T('1',56),T('2',59),T('3',8),T('4',11),T('5',16),T('6',19),T('7',24),T('8',27),T('9',32),
    T(' ',60),T('\n',1),T('\r',1),T('\b',0),T('+',40),T('-',43),T('.',44),T(':',45),T('@',46),T(',',47),
    T(0xa3,48),T(0x2190,57),T(0x2191,54),T('*',49),T(';',50),T('=',53),T('^',54),T('/',55),
#undef A
#undef T
    {'!',56,15},{'"',59,15},{'#',8,15},{'$',11,15},{'%',16,15},{'&',19,15},{'\'',24,15},
    {'(',27,15},{')',32,15},{'<',47,15},{'>',44,15},{'?',55,15},{'[',45,15},{']',50,15}
};
static const rc_combo_desc COMBOS[]={
    {"F2",4,15},{"F4",5,15},{"F6",6,15},{"F8",3,15},
    {"Left",2,15},{"Up",7,15},{"Insert",0,15}
};
static void interrupts(rc_c64 *c){rc_c64_expansion_lines(c);}
static void host_line(void *ctx,void *dev,int pin,int level){
    rc_c64 *c=ctx;
    if(dev==&c->cpu&&pin>=RC_CPU_PORT0){
        c->port_charge=(uint8_t)(((c->port_charge&~c->cpu.ddr)|(c->cpu.port&c->cpu.ddr))&0xc8u);
        rc_c64_media_lines(c);
    }
    if(dev==&c->tape&&pin==RC_DATASETTE_READ)rc_cia6526_input(&c->cia[0],RC_CIA_FLAG,level);
    if(dev==&c->keyboard&&pin==RC_KBD_RESTORE_LINE)c->restore=(uint8_t)level;
    if((dev==&c->vic&&pin==RC_VIC_IRQ)||(dev==&c->cia[0]&&pin==RC_CIA_IRQ)||
       (dev==&c->cia[1]&&pin==RC_CIA_IRQ)||dev==&c->keyboard)interrupts(c);
}
static uint64_t host_now(void *ctx){return ((rc_c64 *)ctx)->sched.now;}
static void host_schedule(void *ctx,void *dev,uint64_t when){rc_sched_set(&((rc_c64 *)ctx)->sched,dev,when);}
static void latch_picture(rc_c64 *c){memcpy(c->shown,c->vic.pixels,sizeof c->shown);c->shown_frames=c->vic.frames;}
static void phi1(void *ctx,uint64_t time){
    (void)time;rc_c64 *c=ctx;rc_vic2_begin(&c->vic);const rc_vic_access a=c->vic.bus[0];
    rc_vic2_clock(&c->vic,0,rc_bus_vic(&c->bus,a.address),c->bus.colour[a.address&1023u]);
}
static void phi2(void *ctx,uint64_t time){
    (void)time;rc_c64 *c=ctx;rc_cia6526_tick(&c->cia[0]);rc_cia6526_tick(&c->cia[1]);
    rc_c64_expansion_tick(c);const rc_reu_pins dma=rc_reu_bus(&c->reu);int accepted=0;uint8_t dma_data=255;
    if(c->vic.aec&&dma.dma&&!c->cpu.pins.write){
        if(c->vic.ba&&dma.valid){if(dma.write)rc_bus_dma_write(&c->bus,dma.address,dma.data);else dma_data=rc_bus_dma_read(&c->bus,dma.address);accepted=1;}
        rc_vic2_clock(&c->vic,1,255,c->bus.colour[c->vic.bus[1].address&1023u]);
    } else if(c->vic.aec){
        const rc_cpu_cycle p=c->cpu.pins;uint8_t data=p.data;
        if(p.write)rc_bus_write(&c->bus,p.address,data);else data=rc_bus_read(&c->bus,p.address,1);
        if(c->trace)c->trace(c->trace_ctx,p.write?RC_TRACE_WRITE:p.sync?RC_TRACE_FETCH:RC_TRACE_READ,p.address,data);
        const uint8_t vic_bus=p.address>=0xd000&&p.address<0xd400&&
            (c->pla.inputs&RC_PLA_CHAREN)?data:255;
        rc_vic2_clock(&c->vic,1,vic_bus,c->bus.colour[c->vic.bus[1].address&1023u]);
        rc_mos6510_clock(&c->cpu,data,c->vic.ba);
    } else {
        const rc_vic_access a=c->vic.bus[1];
        rc_vic2_clock(&c->vic,1,rc_bus_vic(&c->bus,a.address),c->bus.colour[a.address&1023u]);
    }
    rc_reu_tick(&c->reu,c->vic.ba,accepted,dma_data);interrupts(c);
    if(c->vic.frames!=c->shown_frames)latch_picture(c);
    rc_bus_pot_tick(&c->bus);rc_sid_clock(&c->sid,1);rc_c64_media_tick(c);rc_c64_sid_tick(c);
    /* The PAL/NTSC board's mains-derived TOD input is independent of frames. */
    c->tod_at+=c->model>=2?60u:50u;
    if(c->tod_at>=c->hz){c->tod_at-=c->hz;rc_cia6526_tod_pulse(&c->cia[0]);rc_cia6526_tod_pulse(&c->cia[1]);}
}
rc_run_status rc_c64_run(void *self,uint64_t cycles){rc_c64 *c=self;if(rc_c64_sid_paused(c))return RC_RUN_OK;const rc_cycle_host board={c,phi1,phi2,NULL};return rc_sched_cycles(&c->sched,&board,cycles)&&rc_sid_ok(&c->sid)&&rc_datasette_ok(&c->tape)?RC_RUN_OK:RC_RUN_ERROR;}
static void destroy(void *self){rc_c64 *c=self;rc_c64_expansion_release(c);rc_c64_media_release(c);rc_sid_release(&c->sid);free(c);}
static const char *description(const void *self){return MODELS[((const rc_c64 *)self)->model].name;}
static const char *error_text(const void *self){(void)self;return "C64 clock overflow, SID allocation or tape recording failed";}
static void reset(void *self,int power){
    rc_c64 *c=self;
    if(power){for(unsigned i=0;i<65536;++i)c->bus.ram[i]=(uint8_t)((i&64u)?255:0);memset(c->bus.colour,0,1024);rc_sched_init(&c->sched);c->tod_at=0;c->port_charge=0;rc_mos6510_init(&c->cpu,&c->host,RC_MOS6510);}
    else {c->cpu.ddr=0;rc_mos6510_reset(&c->cpu);}
    c->cpu.hooks=&c->hooks;c->tape_audio_phase=0;c->tape_audio_head=c->tape_audio_count=0;
    if(power){rc_vic2_reset(&c->vic);latch_picture(c);c->bus.pot_phase=0;memset(c->bus.pot_value,255,2);memset(c->bus.pot_charge,255,2);for(unsigned p=0;p<2;++p)rc_mouse1351_init(&c->bus.mouse[p]);}rc_cia6526_reset(&c->cia[0]);rc_cia6526_reset(&c->cia[1]);if(power)rc_sid_power(&c->sid);else rc_sid_reset(&c->sid);
    c->keyboard.dev.ops->reset(&c->keyboard);c->restore=0;c->bus.data=c->bus.vic_data=255;c->bus.game=c->bus.exrom=1;
    rc_c1541_reset(&c->drive);c->drive_phase=0;rc_iec_init(&c->iec);rc_c64_iec_lines(c);
    c->drive_audio_mode=power==RC_RESET_AUTOLOAD?RC_DRIVE_AUDIO_IDLE:RC_DRIVE_AUDIO_SELF_TEST;
    rc_fastload_reset(&c->fastload,c->sched.now,c->iec.edges);
    rc_c64_media_lines(c);rc_c64_expansion_reset(c);rc_c64_sid_reset(c);
}
static void nmi(void *self){rc_c64 *c=self;rc_mos6510_input(&c->cpu,RC_CPU_NMI,1);interrupts(c);}
static uint32_t clock_hz(const void *self){return ((const rc_c64 *)self)->hz;}
static uint64_t time_now(const void *self){return ((const rc_c64 *)self)->sched.now;}
static uint64_t period(const void *self){const rc_c64 *c=self;return rc_vic2_cycles_per_line(&c->vic)*rc_vic2_height(&c->vic);}
static void border(void *self,unsigned mode){if(mode<=RC_BORDER_NONE)((rc_c64 *)self)->border=(uint8_t)mode;}
static uint64_t frames(const void *self){return ((const rc_c64 *)self)->vic.frames;}
static int frame(void *self,rc_frame *f){
    rc_c64 *c=self;const unsigned full_h=c->model>=2?247u:272u,raw=rc_vic2_width(&c->vic),raw_h=rc_vic2_height(&c->vic);
    const unsigned w=c->border==RC_BORDER_NONE?320u:c->border==RC_BORDER_MINIMAL?352u:384u;
    const unsigned h=c->border==RC_BORDER_NONE?200u:c->border==RC_BORDER_MINIMAL?232u:full_h;
    const unsigned top=c->border==RC_BORDER_NONE?51u:c->border==RC_BORDER_MINIMAL?35u:c->model>=2?8u:16u;
    const unsigned left=c->border==RC_BORDER_NONE?132u:c->border==RC_BORDER_MINIMAL?116u:100u;
    for(unsigned y=0;y<h;++y)for(unsigned x=0;x<w;++x)c->frame[y*w+x]=c->shown[((y+top)%raw_h)*raw+(x+left)%raw];
    *f=(rc_frame){w,h,w,c->frame,rc_palette_profile("default",c->model>=2),16,w*4u*full_h,h*3u*384u,c->vic.frames};return 1;
}
static size_t text(void *self,char *out,size_t capacity){
    rc_c64 *c=self;size_t n=0;const unsigned bank=(rc_cia6526_port_output(&c->cia[1],0)^3u)&3u;
    const unsigned base=(bank<<14)|((c->vic.reg[24]&240u)<<6);
    for(unsigned y=0;y<25;++y){char row[40];unsigned last=0;
        for(unsigned x=0;x<40;++x){unsigned ch=c->bus.ram[(base+y*40+x)&65535u]&127u;ch=ch<32?ch+64:ch<64?ch:(c->vic.reg[24]&2u)&&ch>=65&&ch<=90?ch:32;row[x]=(char)ch;if(ch!=32)last=x+1;}
        for(unsigned x=0;x<last&&n+1<capacity;++x)out[n++]=row[x];if(n+1<capacity)out[n++]='\n';}
    if(capacity)out[n]=0;return n;
}
static uint32_t audio_rate(const void *self){(void)self;return 48000;}
static size_t audio_pull(void *self,int16_t *out,size_t frames){rc_c64 *c=self;int16_t mono[1024];size_t done=0;while(done<frames){const size_t count=frames-done<1024?frames-done:1024;const size_t n=rc_sid_audio_pull(&c->sid,mono,count);for(size_t i=0;i<n;++i){int sample=mono[i]+rc_c64_tape_audio(c);if(sample>32767)sample=32767;if(sample< -32768)sample= -32768;out[(done+i)*2]=out[(done+i)*2+1]=(int16_t)sample;}done+=n;if(n<count)break;}return done;}
static void key(void *self,unsigned keycode,int down){rc_c64kbd_key(&((rc_c64 *)self)->keyboard,keycode,down);}
static void joystick(void *self,unsigned port,unsigned held){rc_c64kbd_joystick(&((rc_c64 *)self)->keyboard,port,(uint8_t)~held);}
static void pointer(void *self,unsigned port,unsigned type,int x,int y,unsigned buttons){
    rc_c64 *c=self;if(port>=2||type>RC_CONTROL_NONE)return;c->bus.control_type[port]=(uint8_t)type;
    if(type==RC_CONTROL_MOUSE1351){rc_mouse1351_move(&c->bus.mouse[port],x,y,buttons);rc_c64kbd_joystick(&c->keyboard,port,(uint8_t)~rc_mouse1351_buttons(&c->bus.mouse[port]));}
    else if(type==RC_CONTROL_PADDLES){c->bus.paddle[port][0]=(uint8_t)x;c->bus.paddle[port][1]=(uint8_t)y;rc_c64kbd_joystick(&c->keyboard,port,(uint8_t)~((buttons&1u?4u:0)|(buttons&2u?8u:0)));}
    else if(type==RC_CONTROL_NONE)rc_c64kbd_joystick(&c->keyboard,port,255);
}

static uint32_t hashes(const void *self,uint8_t *out,uint32_t max){if(max<4)return 0;memcpy(out,((const rc_c64 *)self)->rom_hashes,128);return 4;}
static void trace(void *self,rc_trace_fn fn,void *ctx){rc_c64 *c=self;c->trace=fn;c->trace_ctx=ctx;}
static int peek(void *self,uint32_t address,uint8_t *value){if(address>65535)return 0;*value=rc_bus_read(&((rc_c64 *)self)->bus,(uint16_t)address,0);return 1;}
static int poke(void *self,uint32_t address,uint8_t value){if(address>65535)return 0;rc_bus_write(&((rc_c64 *)self)->bus,(uint16_t)address,value);return 1;}
static int status(const void *self,rc_machine_status *s){
    const rc_c64 *c=self;const rc_c1541_status d=rc_c1541_get_status(&c->drive);
    s->drive_present=c->drive.enabled;s->drive_device=d.device;s->drive_halftrack=d.halftrack;s->drive_led=d.led;s->drive_motor=d.motor;
    s->tape_present=c->tape.deck.tape.image!=NULL;s->tape_counter=c->tape.deck.tape.position>=20?c->tape.deck.tape.position-20:0;
    s->tape_motor=c->tape.motor;s->tape_play=c->tape.deck.play;s->tape_record=c->tape.deck.record;
    s->sid_loaded=c->sid_session.active;s->sid_song=c->sid_session.song;s->sid_songs=c->sid_session.file.songs;s->sid_paused=c->sid_session.paused;
    s->sid_cycles=c->sid_session.elapsed_cycles;s->sid_hz=c->hz;return 1;
}
static void save(const void *self,rc_state_writer *w){
    const rc_c64 *c=self;rc_c64_media_save(c,w);rc_c64_expansion_save(c,w);rc_c64_sid_save(c,w);rc_sw_begin(w,"C64 ",0);rc_sw_u32(w,1);rc_sw_u64(w,c->sched.now);rc_sw_u64(w,c->tod_at);rc_sw_u8(w,c->restore);rc_sw_u8(w,c->port_charge);rc_sw_end(w);
#define SAVE(tag,object,fn) rc_sw_begin(w,tag,0);fn(&c->object,w);rc_sw_end(w)
    SAVE("BUS ",bus,rc_bus_save);SAVE("CPU ",cpu,rc_mos6510_save);SAVE("VIC2",vic,rc_vic2_save);
    SAVE("SID ",sid,rc_sid_save);SAVE("PLA ",pla,rc_pla_save);SAVE("KBD ",keyboard,rc_c64kbd_save);
#undef SAVE
    for(unsigned i=0;i<2;++i){rc_sw_begin(w,"CIA ",i);rc_cia6526_save(&c->cia[i],w);rc_sw_end(w);}
}
static int load(void *self,rc_state_reader *r){
    rc_c64 *c=self;if(!rc_c64_media_load(c,r)||!rc_c64_expansion_load(c,r)||!rc_c64_sid_load(c,r))return 0;if(!rc_sr_chunk(r,"C64 ",0)||rc_sr_u32(r)!=1)return 0;
    rc_sched_init(&c->sched);c->sched.now=rc_sr_u64(r);c->tod_at=rc_sr_u64(r);c->restore=rc_sr_u8(r);c->port_charge=rc_sr_u8(r);
    if(!rc_sr_ok(r)||c->sched.now==RC_NEVER||c->tod_at>=c->hz||c->restore>1)return 0;
#define LOAD(tag,object,fn) if(!rc_sr_chunk(r,tag,0)||!fn(&c->object,r))return 0
    LOAD("BUS ",bus,rc_bus_load);LOAD("VIC2",vic,rc_vic2_load);
    LOAD("SID ",sid,rc_sid_load);LOAD("PLA ",pla,rc_pla_load);LOAD("KBD ",keyboard,rc_c64kbd_load);
#undef LOAD
    for(unsigned i=0;i<2;++i)if(!rc_sr_chunk(r,"CIA ",i)||!rc_cia6526_load(&c->cia[i],r))return 0;
    if(!rc_sr_chunk(r,"CPU ",0)||!rc_mos6510_load(&c->cpu,r))return 0;
    rc_c64_iec_lines(c);interrupts(c);latch_picture(c);return 1;
}
static const rc_machine_impl IMPL={.destroy=destroy,.description=description,.error=error_text,.reset=reset,.nmi=nmi,
    .clock=clock_hz,.run=rc_c64_run,.time=time_now,.frame_period=period,.frames=frames,.frame=frame,.text=text,
    .audio_rate=audio_rate,.audio_pull=audio_pull,.key=key,.joystick=joystick,.activity=rc_c64_activity,.insert=rc_c64_insert,.media=rc_c64_media,.eject=rc_c64_eject,.control=rc_c64_control,.tape_options=rc_c64_tape_options,.tape_deck=rc_c64_tape_deck,.autoload=rc_c64_autoload,
    .save=save,.load=load,.rom_hashes=hashes,.set_trace=trace,.peek=peek,.poke=poke,.border=border,.pointer=pointer,.status=status};
const rc_family rc_c64_family={.id="c64",.name="Commodore 64",.models=MODELS,.model_count=4,
    .media=MEDIA,.media_count=5,.options=OPTIONS,.option_count=sizeof OPTIONS/sizeof OPTIONS[0],.keys=KEYS,.key_count=65,
    .combos=COMBOS,.combo_count=sizeof COMBOS/sizeof COMBOS[0],.typing=TYPING,.typing_count=sizeof TYPING/sizeof TYPING[0],.joysticks=2,.impl=&IMPL,.create=rc_c64_create};
void *rc_c64_create(const rc_machine_setup *setup,char *error,size_t capacity){
    rc_c64_layout_apply(KEYS,sizeof KEYS/sizeof KEYS[0]);
    rc_c64 *c=calloc(1,sizeof *c);if(!c){if(capacity)snprintf(error,capacity,"Out of memory");return NULL;}
    c->host=(rc_hw_host){c,host_now,host_schedule,host_line};c->model=(uint8_t)setup->model;c->hz=c->model>=2?1022727u:985248u;
    c->bus.cpu=&c->cpu;c->bus.vic=&c->vic;c->bus.cia[0]=&c->cia[0];c->bus.cia[1]=&c->cia[1];
    c->bus.sid=&c->sid;c->bus.pla=&c->pla;c->bus.keyboard=&c->keyboard;
    uint8_t *dest[]={c->bus.kernal,c->bus.basic,c->bus.chargen};const unsigned sizes[]={8192,8192,4096};
    for(unsigned i=0;i<3;++i){const rc_rom_file *rom=rc_rom_index_find(setup->roms,rc_romdb_find(ROMS[i]));
        if(!rom||rom->size!=sizes[i]){if(capacity)snprintf(error,capacity,"Missing %s ROM",ROMS[i]);free(c);return NULL;}
        memcpy(dest[i],rom->data,sizes[i]);rc_sha256(rom->data,sizes[i],c->rom_hashes+i*32);}
    const unsigned vic_models[]={RC_VIC_6569,RC_VIC_8565,RC_VIC_6567,RC_VIC_8562};
    rc_mos6510_init(&c->cpu,&c->host,RC_MOS6510);rc_vic2_init(&c->vic,&c->host,vic_models[c->model]);
    for(unsigned i=0;i<2;++i)rc_cia6526_init(&c->cia[i],&c->host,c->model&1u?RC_CIA_8521:RC_CIA_6526);
    rc_c64kbd_init(&c->keyboard,&c->host);rc_pla_init(&c->pla,&c->host);
    const unsigned sid=setup->options[0];
    if(!rc_sid_init(&c->sid,&c->host,sid,c->hz,48000)){if(capacity)snprintf(error,capacity,"Cannot initialise SID");free(c);return NULL;}
    const char *drive_key=setup->options[4]?"dos1541ii":"dos1541";
    const rc_rom_file *drive_rom=rc_rom_index_find(setup->roms,rc_romdb_find(drive_key));
    if(!drive_rom||drive_rom->size!=16384){if(capacity)snprintf(error,capacity,"Missing %s DOS ROM",setup->options[4]?"1541-II":"1541");rc_sid_release(&c->sid);free(c);return NULL;}
    rc_c1541_init(&c->drive,drive_rom->data,8u+setup->options[1]);rc_sha256(drive_rom->data,drive_rom->size,c->rom_hashes+96);rc_iec_init(&c->iec);
    rc_fastload_init(&c->fastload,c->hz);
    rc_drivesnd_init(&c->drive_sound);
    if(!rc_c64_expansion_init(c,setup,error,capacity)){rc_c64_expansion_release(c);rc_sid_release(&c->sid);free(c);return NULL;}
    rc_c64_media_init(c);reset(c,1);return c;
}
