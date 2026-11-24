/* Original freezer circuits: each cartridge's freeze timing and decoding, as its circuit works. */
#ifndef RC_CART_FREEZER_H
#define RC_CART_FREEZER_H
#include "cart.h"

typedef struct rc_freezer {
    rc_crt *crt;                         /* borrowed; cartridge port owns image */
    uint8_t ram[32768];
    uint16_t hardware;
    uint8_t control, extended, extended_written;
    uint8_t enabled, button, pending, frozen, count, button_age, ba;
} rc_freezer;

int rc_freezer_supported(unsigned hardware);
/* No allocations. Initializes RAM once; reset preserves cartridge RAM. */
int rc_freezer_init(rc_freezer *f, rc_crt *crt);
extern const rc_cart_ops rc_freezer_ops;
void rc_freezer_save(const rc_freezer *f, rc_state_writer *w);
int rc_freezer_load(rc_freezer *f, rc_state_reader *r);
#endif
