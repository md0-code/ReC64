/* CSG 5717 and the 1351 manual: six-bit relative counters, two buttons. */
#ifndef RC_MOUSE1351_H
#define RC_MOUSE1351_H
#include "../core/state.h"
typedef struct rc_mouse1351 {uint8_t x,y,buttons;} rc_mouse1351;
void rc_mouse1351_init(rc_mouse1351 *m);
void rc_mouse1351_move(rc_mouse1351 *m,int dx,int dy,unsigned buttons);
uint8_t rc_mouse1351_pot(const rc_mouse1351 *m,unsigned axis);
unsigned rc_mouse1351_buttons(const rc_mouse1351 *m);
void rc_mouse1351_save(const rc_mouse1351 *m,rc_state_writer *w);
int rc_mouse1351_load(rc_mouse1351 *m,rc_state_reader *r);
#endif
