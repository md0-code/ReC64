#ifndef RC_C64KBD_H
#define RC_C64KBD_H
#include "hw.h"
#include "../core/state.h"
enum {RC_KBD_RESTORE=64,RC_KBD_RESTORE_LINE=0};
typedef struct rc_c64kbd {rc_dev dev;uint8_t row[8],restore,joystick[2];} rc_c64kbd;
extern const char *const rc_c64kbd_names[65];
void rc_c64kbd_init(rc_c64kbd *k,const rc_hw_host *host);
void rc_c64kbd_key(rc_c64kbd *k,unsigned key,int down);
void rc_c64kbd_joystick(rc_c64kbd *k,unsigned port,uint8_t active_low);
/* Resolve the electrical switch network, including reverse scans and ghosting. */
void rc_c64kbd_scan(const rc_c64kbd *k,uint8_t *pa,uint8_t *pb);
void rc_c64kbd_save(const rc_c64kbd *k,rc_state_writer *w);
int rc_c64kbd_load(rc_c64kbd *k,rc_state_reader *r);
#endif
