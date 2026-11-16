/* 250407/250425/250469 address, data, bank-select and keyboard wiring. */
#include "bus.h"
#include "../core/config.h"
static uint8_t decode(rc_bus *b,uint16_t a,int read,int effects) {
    const uint8_t port=rc_mos6510_port_read(b->cpu,1);
    const uint16_t inputs=(uint16_t)((port&7u)<<1|(a&0x8000u)>>10|(a&0x4000u)>>8|
        (a&0x2000u)>>6|(a&0x1000u)>>4|
        (read?RC_PLA_R__W:0)|(b->vic->ba?RC_PLA_BA:0)|
        (b->game?RC_PLA_GAME:0)|(b->exrom?RC_PLA_EXROM:0));
    if(effects)b->pla->inputs=inputs;return rc_pla_decode(inputs);
}
static void inputs(rc_bus *b,unsigned chip,uint8_t pins[2]) {
    uint8_t pa=rc_cia6526_port_output(b->cia[0],0),pb=rc_cia6526_port_output(b->cia[0],1);
    if(!chip){rc_c64kbd_scan(b->keyboard,&pa,&pb);pins[0]=pa;pins[1]=pb;return;}
    /* CIA2 PA6/7 see the inverted, released IEC clock/data through U8. */
    const uint8_t out=rc_cia6526_port_output(b->cia[1],0);
    pins[0]=(uint8_t)(0x3fu|((out&16u)?0:64u)|((out&32u)?0:128u));pins[1]=255;
}
void rc_bus_keyboard(rc_bus *b) {
    for(unsigned chip=0;chip<2;++chip){uint8_t pins[2];inputs(b,chip,pins);
        for(unsigned p=0;p<2;++p)rc_cia6526_port_input(b->cia[chip],p,pins[p]);}
}
uint8_t rc_bus_read(rc_bus *b,uint16_t a,int effects) {
    if(a<2)return rc_mos6510_port_read(b->cpu,a);
    const uint8_t selected=decode(b,a,1,effects);uint8_t data=b->vic_data;
    if(selected&RC_PLA_BASIC)data=b->basic[a&8191u];
    else if(selected&RC_PLA_KERNAL)data=b->kernal[a&8191u];
    else if(selected&RC_PLA_CHAROM)data=b->chargen[a&4095u];
    else if(selected&RC_PLA_I_O){
        if(a<0xd400){
            if(effects)data=rc_vic2_read(b->vic,a&63u);
            else {const unsigned reg=a&63u;const uint8_t saved=reg<47?b->vic->reg[reg]:255;
                data=rc_vic2_read(b->vic,reg);if(reg<47)b->vic->reg[reg]=saved;}
        } else if(a<0xd800){const unsigned reg=a&31u;
            if(reg==25||reg==26)data=effects?rc_sid_pot_read(b->sid,reg,b->pot_value[reg-25]):b->pot_value[reg-25];
            else data=effects?rc_sid_read(b->sid,reg):rc_sid_peek(b->sid,reg);
        }
        else if(a<0xdc00)data=(uint8_t)(b->colour[a&1023u]|(b->vic_data&240u));
        else if(a<0xde00){
            rc_cia6526 *c=b->cia[(a>>8)&1u];
            if(effects){rc_bus_keyboard(b);data=rc_cia6526_read(c,a&15u);}
            else {rc_cia6526 copy=*c;copy.dev.host=NULL;inputs(b,(a>>8)&1u,copy.input);data=rc_cia6526_read(&copy,a&15u);}
        }
    } else if(selected&RC_PLA_CASRAM)data=b->ram[a];
    if(effects)b->data=data;return data;
}
void rc_bus_write(rc_bus *b,uint16_t a,uint8_t data) {
    const uint8_t selected=decode(b,a,0,1);b->data=data;
    if(a<2){rc_mos6510_port_write(b->cpu,a,data);b->ram[a]=b->vic_data;return;}
    if(selected&RC_PLA_CASRAM)b->ram[a]=data;
    if(!(selected&RC_PLA_I_O))return;
    if(a<0xd400)rc_vic2_write(b->vic,a&63u,data);
    else if(a<0xd800)rc_sid_write(b->sid,a&31u,data);
    else if(a<0xdc00)b->colour[a&1023u]=data&15u;
    else if(a<0xde00)rc_cia6526_write(b->cia[(a>>8)&1u],a&15u,data);
}
uint8_t rc_bus_vic(rc_bus *b,uint16_t address) {
    const uint8_t bank=rc_cia6526_port_output(b->cia[1],0)&3u;
    const uint16_t a=(uint16_t)((address&0x3fffu)|((bank^3u)<<14));
    const uint16_t inputs=(uint16_t)(RC_PLA_AEC|RC_PLA_R__W|RC_PLA_GAME|RC_PLA_EXROM|
        (bank&1u?RC_PLA_VA14:0)|(address&0x2000u?RC_PLA_VA13:0)|(address&0x1000u?RC_PLA_VA12:0));
    b->vic_data=rc_pla_decode(inputs)&RC_PLA_CHAROM?b->chargen[a&4095u]:b->ram[a];return b->vic_data;
}
void rc_bus_save(const rc_bus *b,rc_state_writer *w){rc_sw_u32(w,2);rc_sw_u8(w,b->data);rc_sw_u8(w,b->vic_data);rc_sw_u8(w,b->game);rc_sw_u8(w,b->exrom);rc_sw_packed(w,b->ram,sizeof b->ram);rc_sw_packed(w,b->colour,sizeof b->colour);
    rc_sw_u16(w,b->pot_phase);rc_sw_bytes(w,b->pot_value,2);rc_sw_bytes(w,b->pot_charge,2);
    for(unsigned p=0;p<2;++p){rc_sw_u8(w,b->control_type[p]);rc_sw_bytes(w,b->paddle[p],2);rc_mouse1351_save(&b->mouse[p],w);}}
int rc_bus_load(rc_bus *b,rc_state_reader *r){
    const unsigned version=rc_sr_u32(r);if(version<1||version>2)return 0;
    b->data=rc_sr_u8(r);b->vic_data=rc_sr_u8(r);b->game=rc_sr_u8(r);b->exrom=rc_sr_u8(r);rc_sr_packed(r,b->ram,sizeof b->ram);rc_sr_packed(r,b->colour,sizeof b->colour);
    if(version>=2){b->pot_phase=rc_sr_u16(r);rc_sr_bytes(r,b->pot_value,2);rc_sr_bytes(r,b->pot_charge,2);
        for(unsigned p=0;p<2;++p){b->control_type[p]=rc_sr_u8(r);rc_sr_bytes(r,b->paddle[p],2);if(b->control_type[p]>RC_CONTROL_NONE||!rc_mouse1351_load(&b->mouse[p],r))return 0;}}
    else {b->pot_phase=0;for(unsigned p=0;p<2;++p){b->pot_value[p]=b->pot_charge[p]=255;b->control_type[p]=RC_CONTROL_JOYSTICK;rc_mouse1351_init(&b->mouse[p]);}}
    return rc_sr_ok(r)&&b->pot_phase<512&&b->game<=1&&b->exrom<=1;
}
/* SID's 256-cycle discharge and 256-cycle conversion; the 4066 switches
 * follow CIA1 PA6/7. A disconnected input runs to 255. Paddle values are
 * calibrated conversion times; two enabled resistances charge in parallel. */
void rc_bus_pot_tick(rc_bus *b){
    const unsigned phase=b->pot_phase;
    if(!phase)b->pot_charge[0]=b->pot_charge[1]=255;
    if(phase>=256){const uint8_t select=rc_cia6526_port_output(b->cia[0],0);
        for(unsigned axis=0;axis<2;++axis){unsigned threshold=256,resistance=256;
            for(unsigned p=0;p<2;++p)if(select&(64u<<p)){
                const unsigned type=b->control_type[p];
                if(type==RC_CONTROL_MOUSE1351){const unsigned value=rc_mouse1351_pot(&b->mouse[p],axis);if(value<threshold)threshold=value;}
                else if(type==RC_CONTROL_PADDLES){const unsigned value=b->paddle[p][axis];
                    resistance=resistance==256?value:resistance+value?resistance*value/(resistance+value):0;}
            }
            if(resistance<threshold)threshold=resistance;
            if(phase-256>=threshold&&b->pot_charge[axis]==255)b->pot_charge[axis]=(uint8_t)(phase-256);
        }
    }
    if(phase==511){b->pot_value[0]=b->pot_charge[0];b->pot_value[1]=b->pot_charge[1];}
    b->pot_phase=(uint16_t)((phase+1)&511u);
}
