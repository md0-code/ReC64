/* Hardware only: Kontros AR5/6 circuit/PROM, Schoenfeld MK4 and Retro Replay,
 * Giesel FC3 errata and Mantione's measured FC3 freeze timing. */
#include "cart_freezer.h"
#include <string.h>

/* 82S129 truth-table data from MK7PLA.txt, upper half (normal operation).
 * A0=D5 RAM, A1=D1 EXROM, A2=D0 GAME, A3=/IO2, A4=C64 A13,
 * A5=C64 A15, A6=C64 A14, A7=/FREEZE. Outputs O1=/ROM enable,
 * O2=/RAM enable, O3=GAME, O4=EXROM. D2 disables the PROM entirely. */
static const uint8_t ar_prom[128]={
    6,7,15,15,2,3,10,11,6,7,15,15,2,3,10,11,
    6,7,15,15,2,3,10,11,6,7,15,15,2,3,10,11,
    6,7,15,15,2,3,10,11,6,5,15,13,3,1,10,9,
    7,7,15,15,3,3,10,10,7,7,15,15,2,2,11,10,
    6,7,15,15,2,3,10,11,6,7,15,15,2,3,10,11,
    6,7,15,15,2,3,10,11,6,7,15,15,2,3,10,11,
    6,5,14,13,3,1,11,9,7,7,15,15,3,3,11,11,
    7,7,15,15,3,3,10,10,7,7,15,15,3,3,10,10
};

int rc_freezer_supported(unsigned h){
    return h==RC_CRT_ACTION_REPLAY || h==RC_CRT_ACTION_REPLAY4 ||
           h==RC_CRT_RETRO_REPLAY || h==RC_CRT_FINAL_III;
}
static unsigned bank(const rc_freezer *f){
    if(f->hardware==RC_CRT_FINAL_III)return f->control&3u;
    if(f->hardware==RC_CRT_ACTION_REPLAY4)return (f->control&1u)|((f->control>>3)&2u);
    return ((f->control>>3)&3u)|(f->hardware==RC_CRT_RETRO_REPLAY?((f->control>>5)&4u):0u);
}
static uint8_t rom(const rc_freezer *f,unsigned offset,uint8_t bus){
    if(!f->crt || !f->crt->image)return bus;
    const unsigned b=bank(f);
    for(size_t i=0;i<f->crt->count;++i){
        const rc_crt_chip *c=&f->crt->chips[i];
        unsigned start=c->address-0x8000u;
        if(c->bank==b && c->address>=0x8000 && offset>=start && offset-start<c->size)
            return f->crt->image->data[c->offset+offset-start];
    }
    return bus;
}
static uint8_t prom(const rc_freezer *f,uint16_t a,unsigned selected){
    if(!f->enabled)return 15;
    unsigned n=((f->control>>5)&1u)|(f->control&2u)|((f->control&1u)<<2)|
        ((selected&RC_CART_IO2)?0u:8u)|((a>>9)&16u)|((a>>10)&32u)|((a>>8)&64u);
    if(f->frozen)return n>=0x78?10:11;
    return ar_prom[n];
}
/* 0=no driver, 1=ROM, 2=RAM, 3=Retro Replay read register. */
static int decode(const rc_freezer *f,uint16_t a,unsigned s,int vic,unsigned *offset){
    *offset=a&0x1fffu;
    if(f->hardware==RC_CRT_FINAL_III){
        if(s&(RC_CART_IO1|RC_CART_IO2)){*offset=0x1e00u+(a&0x1ffu);return 1;}
        if(s&RC_CART_ROMH){*offset=0x2000u+(a&0x1fffu);return 1;}
        return (s&RC_CART_ROML)?1:0;
    }
    if(!f->enabled)return 0;
    if(f->hardware==RC_CRT_ACTION_REPLAY){
        uint8_t p=prom(f,a,s);
        if(!vic && !(p&2))return 2;
        return !(p&1) && (s&(RC_CART_ROML|RC_CART_ROMH|RC_CART_IO2))?1:0;
    }
    if(f->hardware==RC_CRT_ACTION_REPLAY4){
        if(s&RC_CART_ROMH)return 1;
        if((s&RC_CART_ROML) && (f->control&2))return 1;
        return (s&RC_CART_IO2) && (f->control&2)?1:0;
    }
    /* Retro Replay, flash jumper unset: the flash is hardware write protected. */
    if(s&RC_CART_IO1){
        if((a&255)<2)return 3;
        if(!(f->extended&64))return 0;
        if((f->extended&1) && (a&255)<16)return 0; /* empty clockport */
    }else if(s&RC_CART_IO2){
        if(f->extended&64)return 0;
    }else{
        if(s&RC_CART_ROMH)return 1;
        if(!(s&RC_CART_ROML) || f->frozen)return 0;
        if(f->control&32){*offset+=(bank(f)&3u)*8192;return 2;}
        return (f->control&3)==1?0:1;
    }
    if(f->control&32){
        *offset+=(f->extended&2)?(bank(f)&3u)*8192:0;
        return 2;
    }
    return (f->control&1) && !f->frozen?0:1;
}
static uint8_t drive_mask(const void *ctx,uint16_t a,unsigned selected,int vic){
    unsigned offset;return decode(ctx,a,selected,vic,&offset)?255:0;
}
static void control(rc_freezer *f,uint8_t v){
    if(!f->enabled)return;
    f->control=v;
    if(v&4)f->enabled=0;
    if(f->hardware==RC_CRT_ACTION_REPLAY4){
        if(v&4){f->frozen=0;f->pending=0;}
    }else if(v&64){f->frozen=0;f->pending=0;f->count=0;}
}
static uint8_t read(void *ctx,uint16_t a,unsigned selected,uint8_t bus,int effects,int vic){
    rc_freezer *f=ctx;unsigned offset;
    int d=decode(f,a,selected,vic,&offset);
    uint8_t value=d==1?rom(f,offset,bus):d==2?f->ram[offset]:
        d==3?(uint8_t)((f->control&0x98)|(f->extended&0x42)|(f->button?4:0)):bus;
    /* Both Datel circuits clock IO1 reads too; data is the preceding VIC bus. */
    if(effects && !vic && (selected&RC_CART_IO1) &&
       (f->hardware==RC_CRT_ACTION_REPLAY || f->hardware==RC_CRT_ACTION_REPLAY4))control(f,bus);
    return value;
}
static void write(void *ctx,uint16_t a,unsigned selected,uint8_t value){
    rc_freezer *f=ctx;
    if(f->hardware==RC_CRT_FINAL_III){
        if((selected&RC_CART_IO2) && a==0xdfff && (!(f->control&128)||f->button))f->control=value;
        return;
    }
    if(!f->enabled)return;
    if(selected&RC_CART_IO1){
        if(f->hardware!=RC_CRT_RETRO_REPLAY || !(a&255)){control(f,value);return;}
        if((a&255)==1){
            f->control=(uint8_t)((f->control&~0x98)|(value&0x98));
            if(!f->extended_written){f->extended=(uint8_t)(value&0x46);f->extended_written=1;}
            f->extended=(uint8_t)((f->extended&~1)|(value&1));return;
        }
    }
    unsigned offset;
    if(decode(f,a,selected,0,&offset)==2)f->ram[offset]=value;
}
static rc_cart_lines lines(const void *ctx){
    const rc_freezer *f=ctx;rc_cart_lines l={1,1,1,1,1,f->ba};
    if(f->hardware==RC_CRT_FINAL_III){
        l.game=(uint8_t)((f->control>>5)&1);l.exrom=(uint8_t)((f->control>>4)&1);
        l.nmi=(uint8_t)(((f->control>>6)&1) && !f->button);
        if(f->button && f->count>=7)l.game=0;
    }else{
        if(f->enabled){
            if(f->hardware==RC_CRT_ACTION_REPLAY4){l.game=(f->control>>1)&1;l.exrom=(uint8_t)!(f->control&8);}
            else if(f->frozen){l.game=0;l.exrom=1;}
            else{l.game=(uint8_t)!(f->control&1);l.exrom=(f->control>>1)&1;}
        }
        if(f->pending)l.nmi=l.irq=0;
    }
    return l;
}
static void reset(void *ctx){
    rc_freezer *f=ctx;f->control=f->extended=f->extended_written=0;
    f->button=f->pending=f->frozen=f->count=f->button_age=0;f->ba=1;
    f->enabled=(uint8_t)(f->hardware!=RC_CRT_ACTION_REPLAY4);
}
int rc_freezer_init(rc_freezer *f,rc_crt *crt){
    if(!f || !crt || !rc_freezer_supported(crt->hardware))return 0;
    memset(f,0,sizeof *f);f->crt=crt;f->hardware=crt->hardware;reset(f);return 1;
}
static void observe(void *ctx,uint16_t a,int wr,int effects,int vic){
    rc_freezer *f=ctx;(void)a;
    if(!effects || vic || f->hardware!=RC_CRT_RETRO_REPLAY || !f->pending)return;
    f->count=wr?(uint8_t)(f->count+1):0;
    if(f->count==3){f->pending=0;f->frozen=1;f->enabled=1;f->control=0;}
}
static void tick(void *ctx,uint64_t cycles,int ba){
    rc_freezer *f=ctx;f->ba=(uint8_t)!!ba;
    if(f->button && f->button_age<3){
        uint64_t n=cycles+(uint64_t)f->button_age;f->button_age=(uint8_t)(n<3?n:3);
    }
    if(f->hardware==RC_CRT_FINAL_III){
        if(f->button && ba && f->count<7){
            uint64_t n=cycles+f->count;f->count=(uint8_t)(n<7?n:7);
        }
    }else if(f->hardware!=RC_CRT_RETRO_REPLAY && f->pending){
        if(!ba)f->count=0; /* /LOAD, not count-enable: five consecutive free clocks */
        else if(cycles>=(uint64_t)(5-f->count)){
            f->count=5;f->pending=0;f->frozen=1;f->enabled=1;f->control=0;
        }else f->count=(uint8_t)(f->count+cycles);
    }
}
static void freeze(void *ctx,int pressed){
    rc_freezer *f=ctx;pressed=!!pressed;if(f->button==pressed)return;
    f->button=(uint8_t)pressed;
    if(f->hardware==RC_CRT_FINAL_III){f->count=0;f->button_age=0;return;}
    if(pressed){
        f->button_age=0;
        /* AR5/6's physical button also clears its 74LS273 register. */
        if(f->hardware==RC_CRT_ACTION_REPLAY){f->control=0;f->enabled=1;}
        return;
    }
    if(f->frozen || (f->hardware==RC_CRT_ACTION_REPLAY4 && f->enabled))return;
    if(f->hardware==RC_CRT_RETRO_REPLAY && ((f->extended&4)||f->button_age<3))return;
    f->pending=1;f->count=0;
}
void rc_freezer_save(const rc_freezer *f,rc_state_writer *w){
    rc_sw_u8(w,1);rc_sw_u16(w,f->hardware);rc_sw_u8(w,f->control);
    rc_sw_u8(w,f->extended);rc_sw_u8(w,f->extended_written);rc_sw_u8(w,f->enabled);
    rc_sw_u8(w,f->button);rc_sw_u8(w,f->pending);rc_sw_u8(w,f->frozen);
    rc_sw_u8(w,f->count);rc_sw_u8(w,f->button_age);rc_sw_u8(w,f->ba);
    rc_sw_packed(w,f->ram,sizeof f->ram);
}
int rc_freezer_load(rc_freezer *f,rc_state_reader *r){
    rc_freezer t=*f;
    if(rc_sr_u8(r)!=1 || rc_sr_u16(r)!=f->hardware)return 0;
    t.control=rc_sr_u8(r);t.extended=rc_sr_u8(r);t.extended_written=rc_sr_u8(r);
    t.enabled=rc_sr_u8(r);t.button=rc_sr_u8(r);t.pending=rc_sr_u8(r);t.frozen=rc_sr_u8(r);
    t.count=rc_sr_u8(r);t.button_age=rc_sr_u8(r);t.ba=rc_sr_u8(r);
    rc_sr_packed(r,t.ram,sizeof t.ram);
    if(!rc_sr_ok(r)||t.extended_written>1||t.enabled>1||t.button>1||t.pending>1||
       t.frozen>1||t.count>7||t.button_age>3||t.ba>1||(t.extended&~0x47))return 0;
    *f=t;return 1;
}
static void save_op(const void *ctx,rc_state_writer *w){rc_freezer_save(ctx,w);}
static int load_op(void *ctx,rc_state_reader *r){return rc_freezer_load(ctx,r);}
const rc_cart_ops rc_freezer_ops={observe,drive_mask,reset,read,write,lines,tick,freeze,save_op,load_op};
