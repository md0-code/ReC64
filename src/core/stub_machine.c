/* Diagnostic board: no CPU or firmware. Text, synthetic bus reads and key
 * tones exercise the shell without claiming C64 execution. */
#include "machine_impl.h"
#include "font8x8.h"
#include "../hw/sched.h"
#include "rc_version.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define COLS 40
#define ROWS 25
#define WIDTH 384
#define HEIGHT 272
#define CLOCK 985248u
#define PERIOD (63u * 312u)
#define RATE 48000u
/* Diagnostic colours; the machine's selectable palettes belong to P3. */
static const uint32_t COLORS[16] = {
    0x000000,0xffffff,0x880000,0xaaffee,0xcc44cc,0x00cc55,0x0000aa,0xeeee77,
    0xdd8855,0x664400,0xff7777,0x333333,0x777777,0xaaff66,0x0088ff,0xbbbbbb
};
static const char *const NO_ROMS[] = {NULL};
static const rc_model_desc MODELS[] = {{"test","Foundation test screen",NO_ROMS,NULL}};
static const rc_key_desc KEYS[] = {
#define K(c) {#c,#c,#c,0,0,4,4}
    K(A),K(B),K(C),K(D),K(E),K(F),K(G),K(H),K(I),K(J),K(K),K(L),K(M),
    K(N),K(O),K(P),K(Q),K(R),K(S),K(T),K(U),K(V),K(W),K(X),K(Y),K(Z),
    K(0),K(1),K(2),K(3),K(4),K(5),K(6),K(7),K(8),K(9),
    {"SPACE","Space","SPACE",0,0,4,4},{"ENTER","Return","RETURN",0,0,4,4},
    {"DELETE","Backspace","DEL",0,0,4,4}
#undef K
};
static const rc_typing_desc TYPING[] = {
#define T(c,n) {c,n,RC_KEY_NONE}
    T('A',0),T('B',1),T('C',2),T('D',3),T('E',4),T('F',5),T('G',6),T('H',7),T('I',8),T('J',9),
    T('K',10),T('L',11),T('M',12),T('N',13),T('O',14),T('P',15),T('Q',16),T('R',17),T('S',18),T('T',19),
    T('U',20),T('V',21),T('W',22),T('X',23),T('Y',24),T('Z',25),
    T('a',0),T('b',1),T('c',2),T('d',3),T('e',4),T('f',5),T('g',6),T('h',7),T('i',8),T('j',9),
    T('k',10),T('l',11),T('m',12),T('n',13),T('o',14),T('p',15),T('q',16),T('r',17),T('s',18),T('t',19),
    T('u',20),T('v',21),T('w',22),T('x',23),T('y',24),T('z',25),
    T('0',26),T('1',27),T('2',28),T('3',29),T('4',30),T('5',31),T('6',32),T('7',33),T('8',34),T('9',35),
    T(' ',36),T('\n',37),T('\b',38)
#undef T
};
typedef struct stub {
    rc_sched sched;
    uint8_t ram[65536], font[2048], pixels[WIDTH*HEIGHT], held[39];
    char cells[ROWS][COLS], echo[33];
    uint64_t serial, audio_done, tone_until;
    unsigned sticks[2];
    rc_trace_fn trace;
    void *trace_ctx;
} stub;
static void phi2(void *ctx, uint64_t t) {
    stub *s=ctx;
    if (s->trace) { const uint16_t a=(uint16_t)t; s->trace(s->trace_ctx,RC_TRACE_READ,a,s->ram[a]); }
}
static void *create(const rc_machine_setup *setup, char *error, size_t cap) {
    (void)setup; stub *s=calloc(1,sizeof *s);
    if (!s) { if(cap) snprintf(error,cap,"Out of memory"); return NULL; }
    rc_sched_init(&s->sched); rc_font8x8_build(s->font); return s;
}
static void destroy(void *self) { free(self); }
static const char *description(const void *self) { (void)self; return "Foundation test screen (no C64 CPU)"; }
static const char *error_text(const void *self) { (void)self; return "Guest cycle counter overflow"; }
static void reset(void *self,int power) {
    stub *s=self;
    if(power) { memset(s->ram,0,sizeof s->ram); rc_sched_init(&s->sched); s->audio_done=0; }
    memset(s->held,0,sizeof s->held); memset(s->sticks,0,sizeof s->sticks); s->echo[0]=0; s->tone_until=0;
}
static uint32_t clock_hz(const void *self) { (void)self; return CLOCK; }
static uint64_t time_now(const void *self) { return ((const stub *)self)->sched.now; }
static uint64_t period(const void *self) { (void)self; return PERIOD; }
static rc_run_status run(void *self,uint64_t cycles) {
    stub *s=self; const rc_cycle_host b={s,NULL,phi2,NULL};
    return rc_sched_cycles(&s->sched,&b,cycles) ? RC_RUN_OK:RC_RUN_ERROR;
}
static void put(stub *s,unsigned row,const char *value) {
    size_t n=strlen(value); if(n>COLS) n=COLS; memcpy(s->cells[row],value,n);
}
static void compose(stub *s) {
    memset(s->cells,' ',sizeof s->cells); put(s,1,"             REC64 " RC_VERSION);
    put(s,4,"         FOUNDATION TEST SCREEN"); put(s,6,"   NO C64 CPU OR FIRMWARE IS RUNNING");
    put(s,9,"  HEADLESS SCRIPT AND CONTROL READY"); put(s,12,"       0123456789 ABCDEFGHIJKLMNOP");
    put(s,13,"       QRSTUVWXYZ abcdefghijklmnop"); put(s,16,"  KEY INPUT:"); put(s,18,s->echo);
    char line[COLS+1]; snprintf(line,sizeof line,"  JOYSTICKS: %02X %02X",s->sticks[0],s->sticks[1]); put(s,21,line);
}
static int frame(void *self,rc_frame *f) {
    stub *s=self; compose(s); memset(s->pixels,14,sizeof s->pixels);
    for(unsigned r=0;r<ROWS;++r) for(unsigned c=0;c<COLS;++c)
        for(unsigned y=0;y<8;++y) for(unsigned x=0;x<8;++x) {
            const uint8_t bits=s->font[(uint8_t)s->cells[r][c]*8u+y];
            s->pixels[(36u+r*8u+y)*WIDTH+32u+c*8u+x]=(uint8_t)(bits&(0x80u>>x)?14:6);
        }
    *f=(rc_frame){WIDTH,HEIGHT,WIDTH,s->pixels,COLORS,16,4,3,++s->serial}; return 1;
}
static size_t text(void *self,char *out,size_t cap) {
    stub *s=self; compose(s); size_t n=0;
    for(unsigned r=0;r<ROWS;++r) {
        size_t last=COLS; while(last&&s->cells[r][last-1]==' ') --last;
        for(size_t c=0;c<last&&n+1<cap;++c) out[n++]=s->cells[r][c];
        if(n+1<cap) out[n++]='\n';
    }
    if(cap) out[n]=0;
    return n;
}
static uint32_t audio_rate(const void *self) { (void)self; return RATE; }
static uint64_t samples(uint64_t t) { return (t/CLOCK)*RATE+(t%CLOCK)*RATE/CLOCK; }
static size_t audio_pull(void *self,int16_t *out,size_t count) {
    stub *s=self; const uint64_t due=samples(s->sched.now);
    if(due-s->audio_done>RATE) s->audio_done=due-RATE;
    size_t n=0;
    for(;n<count&&s->audio_done<due;++n,++s->audio_done) {
        const int16_t v=s->audio_done<s->tone_until?(s->audio_done/27u&1u?6000:-6000):0;
        out[n*2]=out[n*2+1]=v;
    }
    return n;
}
static void key(void *self,unsigned k,int down) {
    stub *s=self; if(k>=39) return;
    const int was=s->held[k]; s->held[k]=(uint8_t)(down!=0); if(!down||was) return;
    s->tone_until=samples(s->sched.now)+RATE/10; size_t n=strlen(s->echo);
    if(k==37) { s->echo[0]=0; return; }
    if(k==38) { if(n) s->echo[n-1]=0; return; }
    if(n==sizeof s->echo-1) { memmove(s->echo,s->echo+1,--n); }
    s->echo[n]=k<26?(char)('A'+k):k<36?(char)('0'+k-26):' '; s->echo[n+1]=0;
}
static void joystick(void *self,unsigned port,unsigned bits) { if(port<2) ((stub *)self)->sticks[port]=bits&63u; }
static unsigned activity(const void *self) { (void)self; return 0; }
static int peek(void *self,uint32_t a,uint8_t *v) { if(a>65535) return 0; *v=((stub *)self)->ram[a]; return 1; }
static int poke(void *self,uint32_t a,uint8_t v) { if(a>65535) return 0; ((stub *)self)->ram[a]=v; return 1; }
static void set_trace(void *self,rc_trace_fn fn,void *ctx) { stub *s=self; s->trace=fn;s->trace_ctx=ctx; }
static uint32_t hashes(const void *self,uint8_t *out,uint32_t max) { (void)self;(void)out;(void)max;return 0; }
static void save(const void *self,rc_state_writer *w) {
    const stub *s=self; rc_sw_begin(w,"STUB",0);rc_sw_u32(w,1);
    rc_sw_u64(w,s->sched.now);rc_sw_u64(w,s->audio_done);rc_sw_u64(w,s->tone_until);
    rc_sw_str(w,s->echo);rc_sw_bytes(w,s->held,sizeof s->held);
    rc_sw_u32(w,s->sticks[0]);rc_sw_u32(w,s->sticks[1]);rc_sw_end(w);
    rc_sw_begin(w,"RAM ",0);rc_sw_packed(w,s->ram,sizeof s->ram);rc_sw_end(w);
}
static int load(void *self,rc_state_reader *r) {
    stub *s=self;
    if(!rc_sr_chunk(r,"STUB",0)||r->end-r->at<77||rc_sr_u32(r)!=1) return 0;
    rc_sched_init(&s->sched);s->sched.now=rc_sr_u64(r);s->audio_done=rc_sr_u64(r);s->tone_until=rc_sr_u64(r);
    rc_sr_str(r,s->echo,sizeof s->echo);
    if(!rc_sr_ok(r)||r->end-r->at<47) return 0;
    rc_sr_bytes(r,s->held,sizeof s->held);
    s->sticks[0]=rc_sr_u32(r);s->sticks[1]=rc_sr_u32(r);
    if(s->sched.now==RC_NEVER||s->audio_done>samples(s->sched.now)||!rc_sr_chunk(r,"RAM ",0)) return 0;
    rc_sr_packed(r,s->ram,sizeof s->ram);return rc_sr_ok(r);
}
static const rc_machine_impl IMPL={
    .destroy=destroy,.description=description,.error=error_text,.reset=reset,
    .clock=clock_hz,.run=run,.time=time_now,.frame_period=period,.frame=frame,.text=text,
    .audio_rate=audio_rate,.audio_pull=audio_pull,.key=key,.joystick=joystick,.activity=activity,
    .save=save,.load=load,.rom_hashes=hashes,.peek=peek,.poke=poke,.set_trace=set_trace
};
const rc_family rc_stub_family={
    .id="stub",.name="ReC64",.models=MODELS,.model_count=1,
    .keys=KEYS,.key_count=sizeof KEYS/sizeof KEYS[0],
    .typing=TYPING,.typing_count=sizeof TYPING/sizeof TYPING[0],
    .joysticks=2,.impl=&IMPL,.create=create
};
