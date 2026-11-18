#include "sidplayer.h"
#include <ctype.h>
#include <stdio.h>
#include <string.h>
void rc_sidplayer_init(rc_sidplayer *p){memset(p,0,sizeof *p);rc_tui_init(&p->tui);}
int rc_sidplayer_open(rc_sidplayer *p,const uint8_t *data,size_t size,char *error,size_t cap){rc_sidfile f;if(!rc_sidfile_parse(&f,data,size,error,cap))return 0;p->file=f;p->active=1;return 1;}
void rc_sidplayer_close(rc_sidplayer *p){p->active=0;memset(&p->file,0,sizeof p->file);}
static int key_is(const char *a,const char *b){for(;*a&&*b;++a,++b)if(toupper((unsigned char)*a)!=*b)return 0;return !*a&&!*b;}
unsigned rc_sidplayer_key(rc_sidplayer *p,const char *key,unsigned song,int paused){
    (void)paused;if(!p->active)return RC_SIDPLAYER_NONE;
    if(key_is(key,"ESCAPE"))return RC_SIDPLAYER_CLOSE;
    if(key_is(key,"SPACE"))return RC_SIDPLAYER_PAUSE;
    if(key_is(key,"LEFT")&&song)return RC_SIDPLAYER_PREVIOUS;
    if(key_is(key,"RIGHT")&&song+1<p->file.songs)return RC_SIDPLAYER_NEXT;
    return RC_SIDPLAYER_NONE;
}
/* The TUI font has ASCII glyphs; keep the original metadata bytes in file,
 * and replace only the characters that cannot be drawn. */
static void metadata(rc_tui *t,unsigned row,const char *label,const char *value){char shown[33];unsigned i=0;for(;i<32&&value[i];++i){unsigned c=(unsigned char)value[i];shown[i]=(char)(c>=32&&c<127?c:'?');}shown[i]=0;rc_tui_put(t,row,4,label,14,6);rc_tui_put(t,row+1,4,shown[0]?shown:"(unknown)",1,6);}
void rc_sidplayer_draw(rc_sidplayer *p,unsigned song,uint64_t cycles,uint32_t hz,int paused,const uint32_t palette[16],rc_frame *frame){
    rc_tui *t=&p->tui;char line[64];rc_tui_clear(t);rc_tui_bar(t,0,"SID PLAYER 1/1");rc_tui_box(t,2,2,20,36);
    metadata(t,4,"TITLE",p->file.title);metadata(t,7,"AUTHOR",p->file.author);metadata(t,10,"RELEASE",p->file.released);
    snprintf(line,sizeof line,"%s  SUB-TUNE %u / %u",p->file.rsid?"RSID":"PSID",song+1,p->file.songs);rc_tui_put(t,14,4,line,1,6);
    const uint64_t seconds=hz?cycles/hz:0;snprintf(line,sizeof line,"%02llu:%02llu  %s",(unsigned long long)(seconds/60),(unsigned long long)(seconds%60),paused?"PAUSED":"PLAYING");rc_tui_put(t,16,4,line,1,6);
    rc_tui_put(t,19,4,"SPACE/A PAUSE  ESC/B FILE LIST",14,6);if(p->file.songs>1)rc_tui_put(t,23,4,"LEFT / RIGHT  SUB-TUNE",14,6);
    rc_tui_frame(t,palette,frame);
}
size_t rc_sidplayer_text(rc_sidplayer *p,char *out,size_t cap){return rc_tui_text(&p->tui,out,cap);}
