/* Auto-load types real keyboard input one command at each BASIC prompt. */
#ifndef RC_TYPIST_H
#define RC_TYPIST_H
#include "../core/keymap.h"
typedef struct rc_typist {char commands[512],error[128];unsigned position,length;int active,queued;} rc_typist;
void rc_typist_init(rc_typist *t);
/* Newlines or harness-style \n separators; the last line is executed too. */
int rc_typist_start(rc_typist *t,const char *commands,char *error,size_t capacity);
void rc_typist_cancel(rc_typist *t);
/* Call once per guest frame, before its cycles. 1 queued a line, 0 waiting
 * or finished, -1 failure in t->error. The owner clocks keymap normally. */
int rc_typist_frame(rc_typist *t,rc_machine *machine,rc_keymap *keys);
int rc_typist_busy(const rc_typist *t);
void rc_typist_save(const rc_typist *t,rc_state_writer *w);
int rc_typist_load(rc_typist *t,rc_state_reader *r);
#endif
