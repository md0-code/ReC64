#ifndef RC_SETUP_H
#define RC_SETUP_H
#include "tui.h"
enum {RC_SETUP_MACHINE,RC_SETUP_MEDIA,RC_SETUP_DISPLAY,RC_SETUP_SOUND,RC_SETUP_CONTROLLERS,RC_SETUP_CARTRIDGE,RC_SETUP_STATES,RC_SETUP_ABOUT,RC_SETUP_PAGES};
enum {RC_SETUP_VIDEO=1,RC_SETUP_AUDIO=2,RC_SETUP_RESTART=4,RC_SETUP_RESET=8,RC_SETUP_COLD=16,RC_SETUP_OFF=32,RC_SETUP_CLOSE=64,RC_SETUP_INPUT=128};
typedef struct rc_setup {rc_tui tui;rc_config *config;unsigned page,row,palette_count;int active,crt_available;char palettes[40][128];} rc_setup;
void rc_setup_init(rc_setup *s,rc_config *config);
void rc_setup_palette(rc_setup *s,const char *name);
unsigned rc_setup_key(rc_setup *s,const char *key);
void rc_setup_draw(rc_setup *s,const uint32_t palette[16],rc_frame *frame);
#endif
