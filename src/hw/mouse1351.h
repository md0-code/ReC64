/* CSG 5717 and the 1351 manual: six-bit relative counters, two buttons. */
#ifndef RC_MOUSE1351_H
#define RC_MOUSE1351_H
#include "../core/state.h"
/* A driver reads the counters once a frame and takes the six-bit difference,
 * so more than 31 counts in a frame read as motion the other way. Host motion
 * therefore waits in `pending` and reaches the counters one count at a time,
 * never faster than such a driver follows; what a fast hand adds beyond a
 * short backlog is dropped. Neither is part of a saved state. */
#define RC_MOUSE1351_STEP_CYCLES 700u   /* per count: 28 a PAL frame, 24 an NTSC frame */
#define RC_MOUSE1351_BACKLOG 56
typedef struct rc_mouse1351 {uint8_t x,y,buttons;int16_t pending[2];uint16_t credit[2];} rc_mouse1351;
void rc_mouse1351_init(rc_mouse1351 *m);
void rc_mouse1351_move(rc_mouse1351 *m,int dx,int dy,unsigned buttons);
/* `cycles` CPU cycles have passed. */
void rc_mouse1351_tick(rc_mouse1351 *m,unsigned cycles);
uint8_t rc_mouse1351_pot(const rc_mouse1351 *m,unsigned axis);
unsigned rc_mouse1351_buttons(const rc_mouse1351 *m);
void rc_mouse1351_save(const rc_mouse1351 *m,rc_state_writer *w);
int rc_mouse1351_load(rc_mouse1351 *m,rc_state_reader *r);
#endif
