/* Native file/image browser. Actions leave the dialog open until accepted. */
#ifndef RC_BROWSER_H
#define RC_BROWSER_H
#include "tui.h"
#include "../core/media.h"
#define RC_BROWSER_NAME 256
enum {RC_BROWSER_NONE,RC_BROWSER_CLOSE,RC_BROWSER_LOAD,RC_BROWSER_INSERT,RC_BROWSER_SAVE_PRG,RC_BROWSER_SAVE_TAP,RC_BROWSER_SAVE_D64};
enum {RC_BROWSER_PRG,RC_BROWSER_P00,RC_BROWSER_D64,RC_BROWSER_T64,RC_BROWSER_TAP,RC_BROWSER_CRT,RC_BROWSER_SID,RC_BROWSER_STATE,RC_BROWSER_UNKNOWN};
typedef struct rc_browser_content {uint8_t name[16];uint16_t address;uint32_t length,index;uint8_t type;} rc_browser_content;
typedef int (*rc_browser_tape_list_fn)(void *ctx,const rc_media *image,rc_browser_content *entries,size_t capacity,size_t *count,char *error,size_t cap);
typedef struct rc_browser_result {char path[RC_MEDIA_PATH];uint32_t entry;unsigned action;int overwrite;} rc_browser_result;
typedef struct rc_browser_item {char name[RC_BROWSER_NAME];uint64_t bytes;unsigned folder,format,disabled;rc_browser_content content;} rc_browser_item;
typedef struct rc_browser {
    rc_tui tui;
    rc_browser_item *items;size_t count,selected,top,capacity;
    char path[RC_MEDIA_PATH],image[RC_MEDIA_PATH],filename[RC_BROWSER_NAME],search[RC_BROWSER_NAME],message[128];
    int active,save,contents,roots,confirm,listing_failed,replace_name,states_enabled;unsigned focus,save_type;
    rc_browser_result result;
    rc_browser_tape_list_fn tape_list;void *tape_ctx;
} rc_browser;
void rc_browser_init(rc_browser *b);
void rc_browser_close(rc_browser *b);
void rc_browser_set_tape_reader(rc_browser *b,rc_browser_tape_list_fn reader,void *ctx);
int rc_browser_open(rc_browser *b,const char *folder,int save,char *error,size_t cap);
/* Enters image contents, or returns LOAD for a direct program. */
unsigned rc_browser_file(rc_browser *b,const char *path,char *error,size_t cap);
unsigned rc_browser_key(rc_browser *b,const char *key);
void rc_browser_input(rc_browser *b,const char *utf8);
void rc_browser_error(rc_browser *b,const char *message);
void rc_browser_draw(rc_browser *b,const uint32_t palette[16],rc_frame *frame);
size_t rc_browser_text(rc_browser *b,char *out,size_t capacity);
#endif
