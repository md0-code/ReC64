#ifndef RC_SETUP_H
#define RC_SETUP_H
#include "tui.h"
#include "states.h"
enum {RC_SETUP_MACHINE,RC_SETUP_MEDIA,RC_SETUP_DISPLAY,RC_SETUP_SOUND,RC_SETUP_CONTROLLERS,RC_SETUP_CARTRIDGE,RC_SETUP_STATES,RC_SETUP_ABOUT,RC_SETUP_PAGES};
enum {RC_SETUP_VIDEO=1,RC_SETUP_AUDIO=2,RC_SETUP_RESTART=4,RC_SETUP_RESET=8,RC_SETUP_COLD=16,RC_SETUP_OFF=32,RC_SETUP_CLOSE=64,RC_SETUP_INPUT=128,RC_SETUP_LOAD=256,RC_SETUP_SAVE=512,RC_SETUP_PLAY=1024,RC_SETUP_STOP=2048,RC_SETUP_REWIND=4096,RC_SETUP_RECORD=8192,RC_SETUP_MEDIA_OPTIONS=16384,RC_SETUP_EJECT=32768,RC_SETUP_EJECT_DISK=65536,RC_SETUP_STATE_SAVE=131072,RC_SETUP_STATE_LOAD=262144,RC_SETUP_STATE_REFRESH=524288,RC_SETUP_EJECT_CART=1048576,RC_SETUP_FREEZE=2097152};
typedef struct rc_setup {rc_tui tui;rc_config *config;unsigned page,row,palette_count;int active,crt_available,media_available,has_tape,tape_writable,tape_playing,tape_recording,has_disk,cartridge_available,has_cartridge,can_freeze;char palettes[40][128];
    uint8_t state_status[RC_STATES_SLOTS];char state_time[RC_STATES_SLOTS][20],state_version[RC_STATES_VERSION];rc_states_thumb state_thumb;
} rc_setup;
void rc_setup_init(rc_setup *s,rc_config *config);
void rc_setup_palette(rc_setup *s,const char *name);
unsigned rc_setup_key(rc_setup *s,const char *key);
void rc_setup_draw(rc_setup *s,const uint32_t palette[16],rc_frame *frame);
#endif
