#ifndef RC_TUI_INCLUDED
#define RC_TUI_INCLUDED
#include "../core/machine.h"
#define RC_TUI_COLS 40
#define RC_TUI_ROWS 25
#define RC_TUI_W 384
#define RC_TUI_H 272
typedef struct rc_tui {uint8_t ch[25][40],ink[25][40],paper[25][40],font[2048],pixels[384*272];uint64_t serial;} rc_tui;
void rc_tui_init(rc_tui *t);
void rc_tui_clear(rc_tui *t);
void rc_tui_put(rc_tui *t,unsigned row,unsigned col,const char *text,uint8_t ink,uint8_t paper);
void rc_tui_box(rc_tui *t,unsigned row,unsigned col,unsigned rows,unsigned cols);
void rc_tui_bar(rc_tui *t,unsigned row,const char *title);
void rc_tui_frame(rc_tui *t,const uint32_t palette[16],rc_frame *f);
/* Fit a rendered full-page menu to the machine's current border geometry. */
void rc_tui_fit(rc_tui *t,const rc_frame *guest,rc_frame *out);
/* Draw the first rows over the guest, retaining its geometry and palette. */
int rc_tui_overlay(rc_tui *t,unsigned rows,const rc_frame *guest,rc_frame *out);
size_t rc_tui_text(const rc_tui *t,char *out,size_t capacity);
void rc_tui_status(const uint8_t font[2048],uint32_t *argb,unsigned w,unsigned h,const uint32_t palette[16],const char *text);
#endif
