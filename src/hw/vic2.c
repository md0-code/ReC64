/* MOS 6567 data sheet; Bauer VIC-II article and published VIC Addendum.
 * Cycle numbers are one-based as in the timing diagrams. Pixel coordinates
 * are the sprite coordinates, before the shared eight-dot output delay. */
#include "vic2.h"
#include <string.h>
static inline int ntsc(const rc_vic2 *v){return v->model>=RC_VIC_6567;}
unsigned rc_vic2_cycles_per_line(const rc_vic2 *v){return ntsc(v)?65:63;}
unsigned rc_vic2_width(const rc_vic2 *v){return rc_vic2_cycles_per_line(v)*8;}
unsigned rc_vic2_height(const rc_vic2 *v){return ntsc(v)?263:312;}
static void line(rc_vic2 *v,int pin,int level){if(v->dev.host&&v->dev.host->line)rc_dev_line(&v->dev,pin,level);}
static void irq(rc_vic2 *v){const uint8_t level=(v->reg[25]&v->reg[26]&15u)!=0;if(v->irq!=level){v->irq=level;line(v,RC_VIC_IRQ,level);}}
static void interrupt(rc_vic2 *v,unsigned bit){v->reg[25]|=(uint8_t)bit;irq(v);}
static void raster_compare(rc_vic2 *v){const uint8_t match=v->raster_read==v->compare;if(match&&!v->raster_match)interrupt(v,1);v->raster_match=match;}
static inline unsigned x_at(const rc_vic2 *v,unsigned phase,unsigned dot){
    if(!ntsc(v))return (404u+(v->cycle-1u)*8u+phase*4u+dot)%504u;
    /* The repeated $18c half-cycle coordinates are explicit in Bauer's R8
     * diagram. Other half-cycles follow the nine-bit dot counter. */
    static const unsigned TAIL[8]={0x184,0x18c,0x18c,0x18c,0x18c,0x190,0x194,0x198};
    const unsigned x=v->cycle>=62?TAIL[(v->cycle-62u)*2u+phase]:(412u+(v->cycle-1u)*8u+phase*4u)&511u;
    return (x+dot)&511u;
}
static unsigned pointer_cycle(const rc_vic2 *v,unsigned sprite){return sprite<3?(58u+(ntsc(v)?2u:0u)+2u*sprite):(1u+2u*(sprite-3u));}
static int sprite_phi2(const rc_vic2 *v,unsigned cycle){for(unsigned s=0;s<8;++s)if((v->sprite_dma>>s)&1u){const unsigned p=pointer_cycle(v,s);if(cycle==p||cycle==p+1)return 1;}return 0;}
static inline void vertical_compare(rc_vic2 *v){const unsigned top=v->reg[17]&8u?51:55,bottom=v->reg[17]&8u?251:247;if(v->raster==bottom)v->vertical_border=1;if(v->raster==top&&(v->reg[17]&16u))v->vertical_border=0;}
void rc_vic2_begin(rc_vic2 *v){
    if(v->begun||v->phase)return;v->begun=1;
    if(v->cycle==1){v->c_dma=0;if(v->raster==0){v->vcbase=0;v->refresh=255;v->den=0;v->lp_latched=0;}else {v->raster_read=v->raster;raster_compare(v);}}
    if(v->raster==0&&v->cycle==2){v->raster_read=0;raster_compare(v);if(!v->lp)rc_vic2_lightpen(v,0);}
    if(v->raster==48&&(v->reg[17]&16u))v->den=1;
    v->bad=(uint8_t)(v->raster>=48&&v->raster<=247&&((v->raster&7u)==(v->reg[17]&7u))&&v->den);
    if(v->bad)v->display=1;
    if(v->bad&&v->cycle>=12&&v->cycle<=54)v->c_dma=1;
    if(v->cycle==14){v->vc=v->vcbase;v->vmli=0;if(v->bad)v->rc=0;}
    v->advance|=(uint8_t)~v->reg[23];
    if(v->cycle==16)for(unsigned s=0;s<8;++s){const uint8_t bit=(uint8_t)(1u<<s);if(v->advance&bit){v->mcbase[s]=v->crunch&bit?(uint8_t)((0x2au&(v->mcbase[s]&v->mc[s]))|(0x15u&(v->mcbase[s]|v->mc[s]))):v->mc[s];}if(v->mcbase[s]==63)v->sprite_dma&=(uint8_t)~bit;} 
    if(v->cycle==16)v->crunch=0;
    const unsigned late=ntsc(v)?2u:0u;
    if(v->cycle==55+late||v->cycle==56+late)for(unsigned s=0;s<8;++s){const uint8_t bit=(uint8_t)(1u<<s);if((v->reg[21]&bit)&&v->reg[s*2+1]==(uint8_t)v->raster&&!(v->sprite_dma&bit)){v->sprite_dma|=bit;v->mcbase[s]=0;v->advance|=bit;}}
    if(v->cycle==58+late){for(unsigned s=0;s<8;++s){const uint8_t bit=(uint8_t)(1u<<s);v->mc[s]=v->mcbase[s];if(!(v->sprite_dma&bit))v->sprite_display&=(uint8_t)~bit;else if(v->reg[s*2+1]==(uint8_t)v->raster&&(v->reg[21]&bit))v->sprite_display|=bit;}v->started=0;memset(v->sprite_active,0,8);}
    if(v->cycle==58){if(v->rc==7){v->vcbase=v->vc;v->display=v->bad;}if(v->display)v->rc=(v->rc+1u)&7u;}
    if(v->cycle==rc_vic2_cycles_per_line(v))vertical_compare(v);
    for(unsigned phase=0;phase<2;++phase){v->bus[phase]=(rc_vic_access){0x3fff,phase?RC_VIC_NONE:RC_VIC_IDLE,0};v->sprite_slot[phase]=255;v->sprite_byte[phase]=0;}
    /* Sprite pointers and data are fetched from the end of one line into the start of the next. */
    if(v->cycle<=10||v->cycle>=58)for(unsigned s=0;s<8;++s){const unsigned p=pointer_cycle(v,s);if(v->cycle==p){v->bus[0]=(rc_vic_access){(uint16_t)(((v->reg[24]&0xf0u)<<6)|0x3f8u|s),RC_VIC_POINTER,(uint8_t)s};v->sprite_slot[1]=(uint8_t)s;v->sprite_byte[1]=0;}
        else if(v->cycle==p+1){v->sprite_slot[0]=v->sprite_slot[1]=(uint8_t)s;v->sprite_byte[0]=1;v->sprite_byte[1]=2;}}
    if(v->cycle>=11&&v->cycle<=15)v->bus[0]=(rc_vic_access){(uint16_t)(0x3f00u|v->refresh),RC_VIC_REFRESH,0};
    if(v->cycle>=16&&v->cycle<=55){uint16_t address=0x3fff;const unsigned i=v->vmli<40?v->vmli:0;
        if(v->display)address=v->reg[17]&32u?(uint16_t)(((v->reg[24]&8u)<<10)|(v->vc<<3)|v->rc):(uint16_t)(((v->reg[24]&14u)<<10)|v->matrix[i]<<3|v->rc);
        if(v->reg[17]&64u)address&=0x39ffu;
        v->bus[0]=(rc_vic_access){address,RC_VIC_GRAPHICS,(uint8_t)i};}
    if(v->c_dma&&v->cycle>=15&&v->cycle<=54){const unsigned advance=v->display&&v->cycle>=16?1u:0u;v->bus[1]=(rc_vic_access){(uint16_t)(((v->reg[24]&0xf0u)<<6)|((v->vc+advance)&1023u)),RC_VIC_MATRIX,(uint8_t)((v->vmli+advance)%40u)};}
    for(unsigned phase=0;phase<2;++phase)if(v->sprite_slot[phase]<8){const unsigned s=v->sprite_slot[phase];if(v->sprite_dma&(1u<<s))v->bus[phase]=(rc_vic_access){(uint16_t)(v->pointer[s]<<6|v->mc[s]),RC_VIC_SPRITE,(uint8_t)s};}
    /* On a p-access its pointer is only available at the end of phi1. The
     * first s-access address is updated when that byte has been accepted. */
    int busy=v->c_dma&&v->cycle>=12&&v->cycle<=54;
    if(v->sprite_dma)for(unsigned ahead=0;ahead<4;++ahead){const unsigned c=(v->cycle-1u+ahead)%rc_vic2_cycles_per_line(v)+1u;if(sprite_phi2(v,c))busy=1;}
    const uint8_t ba=(uint8_t)!busy;
    if(ba)v->ba_age=0;else if(v->ba)v->ba_age=0;else if(v->ba_age<255)++v->ba_age;
    if(ba!=v->ba){v->ba=ba;line(v,RC_VIC_BA,ba);}
    const uint8_t aec=(uint8_t)(v->bus[1].kind==RC_VIC_NONE||v->ba_age<3);
    if(aec!=v->aec){v->aec=aec;line(v,RC_VIC_AEC,aec);}
}
static inline uint8_t graphics(rc_vic2 *v,int *foreground){
    const unsigned mcm=(v->reg[22]>>4)&1u,bmm=(v->reg[17]>>5)&1u,ecm=(v->reg[17]>>6)&1u;
    const unsigned pos=v->gpos<8?v->gpos:7;
    const unsigned bit=v->gpos<8?(v->gdata>>(7u-pos))&1u:0;
    const unsigned pair=v->gpos<8?(v->gdata>>(6u-(pos&6u)))&3u:0;
    /* A multicolour text cell without its colour bit 3 is a hires cell: its set bits are the foreground. */
    *foreground=v->vertical_border?0:(int)(mcm&&(bmm||(v->gcolour&8u))?pair&2u:bit);
    uint8_t bg=v->reg[33],out=bg;
    if(ecm&&!bmm)bg=v->reg[33+(v->gchar>>6)];
    if(bmm){if(mcm){const uint8_t colors[4]={v->reg[33],(uint8_t)(v->gchar>>4),(uint8_t)(v->gchar&15u),v->gcolour};out=colors[pair];}else {bg=v->gchar&15u;out=bit?v->gchar>>4:bg;}}
    else if(mcm&&(v->gcolour&8u)){const uint8_t colors[4]={bg,v->reg[34],v->reg[35],(uint8_t)(v->gcolour&7u)};out=colors[pair];}
    else out=bit?v->gcolour:bg;
    v->last_background=bg;
    if(v->vertical_border)out=bg;
    if(ecm&&(bmm||mcm))out=0;
    return out&15u;
}
static inline uint8_t sprite_pixel(rc_vic2 *v,unsigned s,unsigned x){
    const uint8_t bit=(uint8_t)(1u<<s);const unsigned sx=v->reg[s*2]|((v->reg[16]&bit)?256u:0u);
    if((v->sprite_display&bit)&&!(v->started&bit)&&x==sx){v->started|=bit;v->sprite_shift[s]=v->sprite_data[s];v->sprite_pos[s]=0;v->sprite_sub[s]=0;v->sprite_active[s]=1;}
    if(!v->sprite_active[s]||v->sprite_pos[s]>=24)return 0;
    const unsigned expand=(v->reg[29]&bit)?2:1,multi=(v->reg[28]&bit)?2:1,pos=v->sprite_pos[s];
    const unsigned value=multi==2?(v->sprite_shift[s]>>(22u-(pos&~1u)))&3u:(v->sprite_shift[s]>>(23u-pos))&1u;
    if(++v->sprite_sub[s]>=expand*multi){v->sprite_sub[s]=0;v->sprite_pos[s]=(uint8_t)(v->sprite_pos[s]+multi);}
    if(!value)return 0;
    return (uint8_t)((multi==1?v->reg[39+s]:value==1?v->reg[37]:value==2?v->reg[39+s]:v->reg[38])+1u);
}
static inline void pixel(rc_vic2 *v,unsigned x,unsigned at){
    const unsigned left=v->reg[22]&8u?24:31,right=v->reg[22]&8u?344:335;
    if(x==right)v->main_border=1;
    if(x==left){vertical_compare(v);if(!v->vertical_border)v->main_border=0;}
    /* The shifter reloads 4+XSCROLL pixels after the fetch: up to eleven,
     * so with XSCROLL above three the next fetch arrives first. */
    const unsigned reload=4u+(v->reg[22]&7u);
    if(v->held&&v->held_age>=reload){v->gdata=v->held_data;v->gchar=v->held_char;v->gcolour=v->held_colour;v->gpos=0;v->held=0;}
    else if(!v->held&&v->pending&&v->pending_age>=reload){v->gdata=v->pending_data;v->gchar=v->pending_char;v->gcolour=v->pending_colour;v->gpos=0;v->pending=0;}
    int foreground=0;uint8_t color=graphics(v,&foreground),mask=0,first_color=0,first=0;
    /* Only a sprite waiting for its X position or already shifting has anything to do. */
    uint64_t shifting;memcpy(&shifting,v->sprite_active,8);const unsigned waiting=v->sprite_display&(unsigned)~v->started;
    if(shifting||waiting)for(unsigned s=0;s<8;++s){if(!v->sprite_active[s]&&!(waiting&(1u<<s)))continue;const uint8_t sc=sprite_pixel(v,s,x);if(sc){mask|=(uint8_t)(1u<<s);if(!first_color){first_color=sc;first=(uint8_t)s;}}}
    if(mask&&(mask&(mask-1u))){if(!v->reg[30])interrupt(v,4);v->reg[30]|=mask;}
    if(mask&&foreground){if(!v->reg[31])interrupt(v,2);v->reg[31]|=mask;}
    if(first_color&&(!foreground||!(v->reg[27]&(1u<<first))))color=first_color-1u;
    if(v->main_border)color=v->reg[32];
    v->pixels[at]=v->delay[v->delay_at];v->delay[v->delay_at]=color&15u;v->delay_at=(v->delay_at+1u)&7u;
    if(v->gpos<8)++v->gpos;if(v->pending&&v->pending_age<255)++v->pending_age;if(v->held&&v->held_age<255)++v->held_age;
}
void rc_vic2_clock(rc_vic2 *v,unsigned phase,uint8_t data,uint8_t colour){
    if(phase>1||phase!=v->phase||!v->begun)return;
    const rc_vic_access b=v->bus[phase];
    if(b.kind==RC_VIC_POINTER){v->pointer[b.index]=data;if(v->sprite_slot[1]==b.index&&(v->sprite_dma&(1u<<b.index)))v->bus[1].address=(uint16_t)(data<<6|v->mc[b.index]);}
    else if(b.kind==RC_VIC_MATRIX){v->matrix[b.index]=data;v->colour[b.index]=colour&15u;}
    else if(b.kind==RC_VIC_REFRESH)--v->refresh;
    else if(b.kind==RC_VIC_GRAPHICS){if(v->pending){v->held=1;v->held_age=v->pending_age;v->held_data=v->pending_data;v->held_char=v->pending_char;v->held_colour=v->pending_colour;}v->pending=1;v->pending_age=0;v->pending_data=data;v->pending_char=v->display?v->matrix[b.index]:0;v->pending_colour=v->display?v->colour[b.index]:0;if(v->display){v->vc=(v->vc+1u)&1023u;++v->vmli;}}
    if(v->sprite_slot[phase]<8){const unsigned s=v->sprite_slot[phase],byte=v->sprite_byte[phase];if(byte==0)v->sprite_data[s]=(uint32_t)data<<16;else if(byte==1)v->sprite_data[s]=(v->sprite_data[s]&0xff00ffu)|(uint32_t)data<<8;else v->sprite_data[s]=(v->sprite_data[s]&0xffff00u)|data;if(v->sprite_dma&(1u<<s))v->mc[s]=(v->mc[s]+1u)&63u;
        if(phase==0&&v->sprite_slot[1]==s&&v->bus[1].kind==RC_VIC_SPRITE)v->bus[1].address=(uint16_t)(v->pointer[s]<<6|v->mc[s]);}
    const unsigned wrap=ntsc(v)?512u:504u,at=v->raster*rc_vic2_width(v)+(v->cycle-1u)*8u+phase*4u;unsigned x=x_at(v,phase,0);
    for(unsigned i=0;i<4;++i){pixel(v,x,at+i);if(++x==wrap)x=0;}
    if(phase==0){v->phase=1;return;}
    if(v->cycle==56u+(ntsc(v)?2u:0u))v->advance^=v->reg[23]&v->sprite_dma;
    ++v->cycles;v->phase=0;v->begun=0;
    if(++v->cycle>rc_vic2_cycles_per_line(v)){v->cycle=1;if(++v->raster>=rc_vic2_height(v)){v->raster=0;++v->frames;}}
}
uint8_t rc_vic2_read(rc_vic2 *v,unsigned reg){reg&=63u;if(reg>=47)return 255;if(reg==17)return (uint8_t)((v->reg[17]&127u)|((v->raster_read&256u)>>1));if(reg==18)return (uint8_t)v->raster_read;if(reg==22)return v->reg[22]|192u;if(reg==24)return v->reg[24]|1u;if(reg==25)return (uint8_t)(v->reg[25]|112u|(v->irq?128u:0u));if(reg==26)return v->reg[26]|240u;if(reg>=32)return v->reg[reg]|240u;const uint8_t value=v->reg[reg];if(reg==30||reg==31)v->reg[reg]=0;return value;}
void rc_vic2_write(rc_vic2 *v,unsigned reg,uint8_t value){reg&=63u;if(reg>=47||reg==19||reg==20||reg==30||reg==31)return;
    if(reg==25){v->reg[25]&=(uint8_t)~(value&15u);irq(v);return;}
    if(reg==26){v->reg[26]=value&15u;irq(v);return;}
    if(reg==17)v->compare=(uint16_t)((v->compare&255u)|((value&128u)<<1));
    if(reg==18){v->compare=(uint16_t)((v->compare&256u)|value);raster_compare(v);return;}
    if(reg==23){const uint8_t cleared=v->reg[23]&(uint8_t)~value;if(v->cycle==15&&v->phase==1)v->crunch|=cleared&(uint8_t)~v->advance;v->advance|=cleared;}
    if(reg>=32)value&=15u;
    v->reg[reg]=value;if(reg==17)raster_compare(v);
}
void rc_vic2_lightpen(rc_vic2 *v,int level){level=level!=0;if(!level&&(!v->lp_latched)&&!(v->raster==311&&!ntsc(v))){v->reg[19]=(uint8_t)(x_at(v,1,4)>>1);v->reg[20]=(uint8_t)v->raster;v->lp_latched=1;interrupt(v,8);}v->lp=(uint8_t)level;}
void rc_vic2_reset(rc_vic2 *v){const rc_dev dev=v->dev;const uint8_t model=v->model;memset(v,0,sizeof *v);v->dev=dev;v->model=model;v->cycle=1;v->main_border=v->vertical_border=v->ba=v->aec=v->lp=1;v->gpos=8;v->refresh=255;line(v,RC_VIC_IRQ,0);line(v,RC_VIC_BA,1);line(v,RC_VIC_AEC,1);}
void rc_vic2_save(const rc_vic2 *v,rc_state_writer *w){
    rc_sw_u32(w,2);rc_sw_u64(w,v->cycles);rc_sw_u64(w,v->frames);rc_sw_u16(w,v->raster);rc_sw_u16(w,v->raster_read);rc_sw_u16(w,v->compare);rc_sw_u16(w,v->vc);rc_sw_u16(w,v->vcbase);
#define B(n) rc_sw_u8(w,v->n)
    B(model);B(cycle);B(phase);B(begun);rc_sw_bytes(w,v->reg,47);rc_sw_bytes(w,v->matrix,40);rc_sw_bytes(w,v->colour,40);
    B(rc);B(vmli);B(refresh);B(display);B(den);B(bad);B(c_dma);B(ba);B(aec);B(ba_age);B(irq);B(raster_match);B(lp);B(lp_latched);B(sprite_dma);B(sprite_display);B(advance);B(crunch);B(started);
    rc_sw_bytes(w,v->mc,8);rc_sw_bytes(w,v->mcbase,8);rc_sw_bytes(w,v->pointer,8);rc_sw_bytes(w,v->sprite_pos,8);rc_sw_bytes(w,v->sprite_sub,8);rc_sw_bytes(w,v->sprite_active,8);
    for(unsigned i=0;i<8;++i){rc_sw_u32(w,v->sprite_data[i]);rc_sw_u32(w,v->sprite_shift[i]);}
    B(gdata);B(gpos);B(gchar);B(gcolour);B(pending_data);B(pending_char);B(pending_colour);B(pending_age);B(pending);B(main_border);B(vertical_border);rc_sw_bytes(w,v->delay,8);B(delay_at);B(last_background);
    B(held_data);B(held_char);B(held_colour);B(held_age);B(held);
    for(unsigned i=0;i<2;++i){rc_sw_u16(w,v->bus[i].address);rc_sw_u8(w,v->bus[i].kind);rc_sw_u8(w,v->bus[i].index);B(sprite_slot[i]);B(sprite_byte[i]);}
    rc_sw_packed(w,v->pixels,RC_VIC_MAX_PIXELS);
#undef B
}
int rc_vic2_load(rc_vic2 *v,rc_state_reader *r){
    if(r->end-r->at<329)return 0;const uint32_t version=rc_sr_u32(r);if(version!=1&&version!=2)return 0;v->cycles=rc_sr_u64(r);v->frames=rc_sr_u64(r);v->raster=rc_sr_u16(r);v->raster_read=rc_sr_u16(r);v->compare=rc_sr_u16(r);v->vc=rc_sr_u16(r);v->vcbase=rc_sr_u16(r);
#define B(n) v->n=rc_sr_u8(r)
    B(model);B(cycle);B(phase);B(begun);rc_sr_bytes(r,v->reg,47);rc_sr_bytes(r,v->matrix,40);rc_sr_bytes(r,v->colour,40);
    B(rc);B(vmli);B(refresh);B(display);B(den);B(bad);B(c_dma);B(ba);B(aec);B(ba_age);B(irq);B(raster_match);B(lp);B(lp_latched);B(sprite_dma);B(sprite_display);B(advance);B(crunch);B(started);
    rc_sr_bytes(r,v->mc,8);rc_sr_bytes(r,v->mcbase,8);rc_sr_bytes(r,v->pointer,8);rc_sr_bytes(r,v->sprite_pos,8);rc_sr_bytes(r,v->sprite_sub,8);rc_sr_bytes(r,v->sprite_active,8);
    for(unsigned i=0;i<8;++i){v->sprite_data[i]=rc_sr_u32(r);v->sprite_shift[i]=rc_sr_u32(r);}
    B(gdata);B(gpos);B(gchar);B(gcolour);B(pending_data);B(pending_char);B(pending_colour);B(pending_age);B(pending);B(main_border);B(vertical_border);rc_sr_bytes(r,v->delay,8);B(delay_at);B(last_background);
    /* Version 1 had no second waiting fetch. */
    if(version>=2){B(held_data);B(held_char);B(held_colour);B(held_age);B(held);}else v->held=0;
    for(unsigned i=0;i<2;++i){v->bus[i].address=rc_sr_u16(r);v->bus[i].kind=rc_sr_u8(r);v->bus[i].index=rc_sr_u8(r);B(sprite_slot[i]);B(sprite_byte[i]);}
    rc_sr_packed(r,v->pixels,RC_VIC_MAX_PIXELS);
#undef B
    if(v->model>3||!v->cycle||v->cycle>rc_vic2_cycles_per_line(v)||v->raster>=rc_vic2_height(v)||v->phase>1||v->delay_at>7||v->held>1)return 0;line(v,RC_VIC_IRQ,v->irq);line(v,RC_VIC_BA,v->ba);line(v,RC_VIC_AEC,v->aec);return rc_sr_ok(r);
}
static uint8_t dev_read(void *d,unsigned sp,uint32_t off){(void)sp;return rc_vic2_read(d,off);}static void dev_write(void *d,unsigned sp,uint32_t off,uint8_t value){(void)sp;rc_vic2_write(d,off,value);}
static void dev_reset(void *d){rc_vic2_reset(d);}static void dev_save(const void *d,rc_state_writer *w){rc_vic2_save(d,w);}static int dev_load(void *d,rc_state_reader *r){return rc_vic2_load(d,r);}
static const rc_dev_ops OPS={.read=dev_read,.write=dev_write,.reset=dev_reset,.save=dev_save,.load=dev_load};
void rc_vic2_init(rc_vic2 *v,const rc_hw_host *host,unsigned model){memset(v,0,sizeof *v);v->dev=(rc_dev){&OPS,host,"VIC-II"};v->model=(uint8_t)model;rc_vic2_reset(v);}
