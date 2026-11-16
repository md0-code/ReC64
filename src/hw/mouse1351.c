#include "mouse1351.h"
void rc_mouse1351_init(rc_mouse1351 *m){m->x=m->y=m->buttons=0;}
void rc_mouse1351_move(rc_mouse1351 *m,int dx,int dy,unsigned buttons){
    m->x=(uint8_t)((m->x+(unsigned)dx)&63u);
    /* Positive POTY moves up in Commodore's supplied driver. */
    m->y=(uint8_t)((m->y-(unsigned)dy)&63u);m->buttons=(uint8_t)(buttons&3u);
}
uint8_t rc_mouse1351_pot(const rc_mouse1351 *m,unsigned axis){return (uint8_t)((axis?m->y:m->x)<<1);}
unsigned rc_mouse1351_buttons(const rc_mouse1351 *m){return (m->buttons&1u?16u:0)|(m->buttons&2u?1u:0);}
void rc_mouse1351_save(const rc_mouse1351 *m,rc_state_writer *w){rc_sw_u32(w,1);rc_sw_u8(w,m->x);rc_sw_u8(w,m->y);rc_sw_u8(w,m->buttons);}
int rc_mouse1351_load(rc_mouse1351 *m,rc_state_reader *r){if(rc_sr_u32(r)!=1)return 0;m->x=rc_sr_u8(r);m->y=rc_sr_u8(r);m->buttons=rc_sr_u8(r);return rc_sr_ok(r)&&m->x<64&&m->y<64&&m->buttons<4;}
