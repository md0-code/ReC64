/* VIC-II: board calls begin once, supplies each half-cycle's bus byte, then
 * clock(phase). The chip never accesses memory itself. For a non-owned phi2,
 * supply the VIC internal bus (CPU VIC-register data, otherwise $ff).
 * Addresses are within the VIC's 16 KiB bank; colour is a separate nybble. */
#ifndef RC_VIC2_H
#define RC_VIC2_H
#include "hw.h"
#include "../core/state.h"
enum {RC_VIC_6569,RC_VIC_8565,RC_VIC_6567,RC_VIC_8562};
enum {RC_VIC_NONE,RC_VIC_IDLE,RC_VIC_REFRESH,RC_VIC_MATRIX,RC_VIC_GRAPHICS,RC_VIC_POINTER,RC_VIC_SPRITE};
enum {RC_VIC_IRQ,RC_VIC_BA,RC_VIC_AEC};
#define RC_VIC_MAX_PIXELS (520u*312u)
typedef struct rc_vic_access {uint16_t address;uint8_t kind,index;} rc_vic_access;
typedef struct rc_vic2 {
    rc_dev dev;
    uint64_t cycles,frames;
    uint16_t raster,raster_read,compare,vc,vcbase;
    uint8_t model,cycle,phase,begun,reg[47],matrix[40],colour[40],rc,vmli,refresh;
    uint8_t display,den,bad,c_dma,ba,aec,ba_age,irq,raster_match,lp,lp_latched;
    uint8_t sprite_dma,sprite_display,advance,crunch,started;
    uint8_t mc[8],mcbase[8],pointer[8],sprite_pos[8],sprite_sub[8],sprite_active[8];
    uint32_t sprite_data[8],sprite_shift[8];
    uint8_t gdata,gpos,gchar,gcolour,pending_data,pending_char,pending_colour,pending_age,pending;
    /* A fetch still waiting for its XSCROLL delay when the next one arrives. */
    uint8_t held_data,held_char,held_colour,held_age,held;
    uint8_t main_border,vertical_border,delay[8],delay_at,last_background;
    rc_vic_access bus[2];
    uint8_t sprite_slot[2],sprite_byte[2];
    /* Host policy, never saved. With economy on only one frame in four is drawn; `published` tells whether
     * the frame that just ended was. Collisions, interrupts and bus timing do not depend on it. */
    uint8_t economy,skip,published;
    uint8_t pixels[RC_VIC_MAX_PIXELS];
} rc_vic2;
void rc_vic2_init(rc_vic2 *v,const rc_hw_host *host,unsigned model);
void rc_vic2_reset(rc_vic2 *v);
unsigned rc_vic2_width(const rc_vic2 *v);
unsigned rc_vic2_height(const rc_vic2 *v);
unsigned rc_vic2_cycles_per_line(const rc_vic2 *v);
void rc_vic2_begin(rc_vic2 *v);
void rc_vic2_clock(rc_vic2 *v,unsigned phase,uint8_t data,uint8_t colour);
uint8_t rc_vic2_read(rc_vic2 *v,unsigned reg);
void rc_vic2_write(rc_vic2 *v,unsigned reg,uint8_t value);
void rc_vic2_lightpen(rc_vic2 *v,int level);
void rc_vic2_save(const rc_vic2 *v,rc_state_writer *w);
int rc_vic2_load(rc_vic2 *v,rc_state_reader *r);
#endif
