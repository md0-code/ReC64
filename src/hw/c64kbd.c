/* Keyboard switch matrix from the C64 circuit diagram and reference guide. */
#include "c64kbd.h"
#include <string.h>
const char *const rc_c64kbd_names[65]={
    "DELETE","RETURN","CURSOR RIGHT","F7","F1","F3","F5","CURSOR DOWN",
    "3","W","A","4","Z","S","E","LEFT SHIFT",
    "5","R","D","6","C","F","T","X",
    "7","Y","G","8","B","H","U","V",
    "9","I","J","0","M","K","O","N",
    "+","P","L","-",".",":","@",",",
    "POUND","*",";","HOME","RIGHT SHIFT","=","UP ARROW","/",
    "1","LEFT ARROW","CTRL","2","SPACE","COMMODORE","Q","RUN STOP","RESTORE"
};
void rc_c64kbd_key(rc_c64kbd *k,unsigned key,int down){if(key<64){if(down)k->row[key/8]|=(uint8_t)(1u<<(key&7u));else k->row[key/8]&=(uint8_t)~(1u<<(key&7u));}else if(key==64){k->restore=(uint8_t)(down!=0);if(k->dev.host&&k->dev.host->line)rc_dev_line(&k->dev,RC_KBD_RESTORE_LINE,k->restore);}}
void rc_c64kbd_joystick(rc_c64kbd *k,unsigned port,uint8_t pins){if(port<2)k->joystick[port]=pins|0xe0u;}
void rc_c64kbd_scan(const rc_c64kbd *k,uint8_t *pa,uint8_t *pb){uint8_t a=*pa&k->joystick[1],b=*pb&k->joystick[0],old_a,old_b;do{old_a=a;old_b=b;for(unsigned r=0;r<8;++r){if(!(a&(1u<<r)))b&=(uint8_t)~k->row[r];if(k->row[r]&(uint8_t)~b)a&=(uint8_t)~(1u<<r);}}while(old_a!=a||old_b!=b);*pa=a;*pb=b;}
void rc_c64kbd_save(const rc_c64kbd *k,rc_state_writer *w){rc_sw_u32(w,1);rc_sw_bytes(w,k->row,8);rc_sw_u8(w,k->restore);rc_sw_bytes(w,k->joystick,2);}
int rc_c64kbd_load(rc_c64kbd *k,rc_state_reader *r){if(r->end-r->at<15||rc_sr_u32(r)!=1)return 0;rc_sr_bytes(r,k->row,8);k->restore=rc_sr_u8(r);rc_sr_bytes(r,k->joystick,2);if(k->dev.host&&k->dev.host->line)rc_dev_line(&k->dev,RC_KBD_RESTORE_LINE,k->restore);return rc_sr_ok(r)&&k->restore<=1;}
static void reset(void *d){rc_c64kbd *k=d;memset(k->row,0,8);k->joystick[0]=k->joystick[1]=255;rc_c64kbd_key(k,64,0);}
static void save(const void *d,rc_state_writer *w){rc_c64kbd_save(d,w);}static int load(void *d,rc_state_reader *r){return rc_c64kbd_load(d,r);}
static const rc_dev_ops OPS={.reset=reset,.save=save,.load=load};
void rc_c64kbd_init(rc_c64kbd *k,const rc_hw_host *host){memset(k,0,sizeof *k);k->dev=(rc_dev){&OPS,host,"Keyboard"};reset(k);}
