#include "mouse1351.h"
void rc_mouse1351_init(rc_mouse1351 *m){m->x=m->y=m->buttons=0;m->pending[0]=m->pending[1]=0;m->credit[0]=m->credit[1]=RC_MOUSE1351_STEP_CYCLES;}
static int16_t backlog(int value){return (int16_t)(value>RC_MOUSE1351_BACKLOG?RC_MOUSE1351_BACKLOG:value< -RC_MOUSE1351_BACKLOG?-RC_MOUSE1351_BACKLOG:value);}
void rc_mouse1351_move(rc_mouse1351 *m,int dx,int dy,unsigned buttons){
    if(dx>2*RC_MOUSE1351_BACKLOG)dx=2*RC_MOUSE1351_BACKLOG;if(dx< -2*RC_MOUSE1351_BACKLOG)dx=-2*RC_MOUSE1351_BACKLOG;
    if(dy>2*RC_MOUSE1351_BACKLOG)dy=2*RC_MOUSE1351_BACKLOG;if(dy< -2*RC_MOUSE1351_BACKLOG)dy=-2*RC_MOUSE1351_BACKLOG;
    m->pending[0]=backlog(m->pending[0]+dx);
    /* Positive POTY moves up in Commodore's supplied driver. */
    m->pending[1]=backlog(m->pending[1]-dy);m->buttons=(uint8_t)(buttons&3u);
}
void rc_mouse1351_tick(rc_mouse1351 *m,unsigned cycles){
    for(unsigned axis=0;axis<2;++axis){
        /* At rest the first count of new motion goes out at once. */
        if(!m->pending[axis]){m->credit[axis]=RC_MOUSE1351_STEP_CYCLES;continue;}
        unsigned credit=m->credit[axis]+cycles;uint8_t *counter=axis?&m->y:&m->x;
        while(credit>=RC_MOUSE1351_STEP_CYCLES&&m->pending[axis]){
            credit-=RC_MOUSE1351_STEP_CYCLES;
            if(m->pending[axis]>0){--m->pending[axis];*counter=(uint8_t)((*counter+1u)&63u);}
            else {++m->pending[axis];*counter=(uint8_t)((*counter+63u)&63u);}
        }
        m->credit[axis]=(uint16_t)(credit>RC_MOUSE1351_STEP_CYCLES?RC_MOUSE1351_STEP_CYCLES:credit);
    }
}
uint8_t rc_mouse1351_pot(const rc_mouse1351 *m,unsigned axis){return (uint8_t)((axis?m->y:m->x)<<1);}
unsigned rc_mouse1351_buttons(const rc_mouse1351 *m){return (m->buttons&1u?16u:0)|(m->buttons&2u?1u:0);}
void rc_mouse1351_save(const rc_mouse1351 *m,rc_state_writer *w){rc_sw_u32(w,1);rc_sw_u8(w,m->x);rc_sw_u8(w,m->y);rc_sw_u8(w,m->buttons);}
int rc_mouse1351_load(rc_mouse1351 *m,rc_state_reader *r){if(rc_sr_u32(r)!=1)return 0;m->x=rc_sr_u8(r);m->y=rc_sr_u8(r);m->buttons=rc_sr_u8(r);m->pending[0]=m->pending[1]=0;m->credit[0]=m->credit[1]=RC_MOUSE1351_STEP_CYCLES;return rc_sr_ok(r)&&m->x<64&&m->y<64&&m->buttons<4;}
