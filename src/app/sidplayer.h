/* SID player, drawn in the same character-cell interface as setup. */
#ifndef RC_SIDPLAYER_H
#define RC_SIDPLAYER_H
#include "tui.h"
#include "../core/media/sidfile.h"
enum { RC_SIDPLAYER_NONE,RC_SIDPLAYER_CLOSE,RC_SIDPLAYER_PREVIOUS,RC_SIDPLAYER_NEXT,RC_SIDPLAYER_PAUSE };
typedef struct rc_sidplayer {rc_tui tui;rc_sidfile file;int active;} rc_sidplayer;
void rc_sidplayer_init(rc_sidplayer *p);
int rc_sidplayer_open(rc_sidplayer *p,const uint8_t *data,size_t size,char *error,size_t cap);
void rc_sidplayer_close(rc_sidplayer *p);
unsigned rc_sidplayer_key(rc_sidplayer *p,const char *key,unsigned song,int paused);
void rc_sidplayer_draw(rc_sidplayer *p,unsigned song,uint64_t cycles,uint32_t hz,int paused,const uint32_t palette[16],rc_frame *frame);
size_t rc_sidplayer_text(rc_sidplayer *p,char *out,size_t cap);
#endif
