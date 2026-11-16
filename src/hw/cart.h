/* Expansion-port selected inputs and physical output levels, C64 schematics. */
#ifndef RC_CART_H
#define RC_CART_H
#include "../core/media/crt.h"
enum { RC_CART_ROML=1, RC_CART_ROMH=2, RC_CART_IO1=4, RC_CART_IO2=8 };
typedef struct rc_cart_lines { uint8_t game,exrom,nmi,irq,dma,ba; } rc_cart_lines;
/* Physical levels: 1 means released/high. ba is the last board BA input.
 * Selected flags mean active-low inputs are asserted. read(side_effects=0)
 * must not change state. vic distinguishes VIC phi1 from CPU phi2 access. */
typedef struct rc_cart_ops {
    void (*observe)(void *ctx,uint16_t address,int write,int side_effects,int vic);
    void (*reset)(void *ctx);
    uint8_t (*read)(void *ctx,uint16_t address,unsigned selected,uint8_t bus,int side_effects,int vic);
    void (*write)(void *ctx,uint16_t address,unsigned selected,uint8_t value);
    rc_cart_lines (*lines)(const void *ctx);
    void (*tick)(void *ctx,uint64_t cycles,int ba);
    void (*freeze)(void *ctx,int pressed);
    void (*save)(const void *ctx,rc_state_writer *w);
    int (*load)(void *ctx,rc_state_reader *r);
} rc_cart_ops;
typedef struct rc_cart {
    rc_crt crt;
    const rc_cart_ops *ops; void *ctx; /* external mapper, board owned */
    uint16_t bank;
    uint8_t control,locked,ba;
} rc_cart;
void rc_cart_init(rc_cart *c);
void rc_cart_close(rc_cart *c);
/* Parses every CRT type; unsupported hardware requires bind before running. */
int rc_cart_attach(rc_cart *c,rc_media *image,char *error,size_t cap);
int rc_cart_supported(unsigned hardware);
void rc_cart_bind(rc_cart *c,const rc_cart_ops *ops,void *ctx);
void rc_cart_reset(rc_cart *c);
/* Board calls before PLA decode; vector-sensitive freezers can change lines. */
void rc_cart_observe(rc_cart *c,uint16_t address,int write,int side_effects,int vic);
rc_cart_lines rc_cart_get_lines(const rc_cart *c);
uint8_t rc_cart_read(rc_cart *c,uint16_t address,unsigned selected,uint8_t bus,int side_effects,int vic);
void rc_cart_write(rc_cart *c,uint16_t address,unsigned selected,uint8_t value);
void rc_cart_tick(rc_cart *c,uint64_t cycles,int ba);
void rc_cart_freeze(rc_cart *c,int pressed);
void rc_cart_save(const rc_cart *c,rc_state_writer *w);
/* External ops must already be bound to the matching mapper. */
int rc_cart_load(rc_cart *c,rc_state_reader *r);
#endif
