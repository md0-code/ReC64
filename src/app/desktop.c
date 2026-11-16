/* The host loop. Headless runs never initialise SDL video or create a window. */
#include "desktop.h"
#include "harness.h"
#include "pace.h"
#include "setup.h"
#include "browser.h"
#include "typist.h"
#include "../core/media/tapedeck.h"
#include "../core/media/d64.h"
#include "pads.h"
#include "joykeys.h"
#include "../core/keymap.h"
#include "../core/palette.h"
#include "../core/yamledit.h"
#include "../host/host.h"
#include "../platform/video.h"
#include "../platform/png.h"
#include "../platform/audio.h"
#include "../platform/overlay.h"
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
typedef struct desktop {
    rc_machine *machine;
    rc_video *video;
    rc_host_stream *tcp;
    rc_host_audio audio;
    rc_wav wav;
    rc_config *config;const rc_rom_index *roms;rc_machine **owner;
    rc_pads pads;rc_joykeys joykeys;rc_keymap keymap;int suppress_text;
    rc_media *media[RC_CONFIG_MEDIA];uint32_t media_entry[RC_CONFIG_MEDIA];
    rc_browser browser;rc_typist typist;
    rc_setup setup;rc_osd osd;uint32_t palette[16];
    char palette_folders[3][RC_HOST_PATH];unsigned palette_folders_count;
    int hidden,paused,quit,restart_pending,pace_dirty;uint64_t frames;rc_config running_config;
    uint32_t *pixels;
    size_t pixel_count;
    char input[1024];
    size_t input_size;
    int stdin_control, full, io_failed,source_slot;
} desktop;
static void media_status(desktop *d);
static rc_machine *machine(void *ctx) { return ((desktop *)ctx)->machine; }
static int message_active(const desktop *d){return d->osd.text[0]&&(int32_t)(d->osd.until_ms-(uint32_t)(d->frames*20u))>=0;}
static void message(desktop *d,const char *text){rc_osd_show(&d->osd,(uint32_t)(d->frames*20u),"%s",text);}
static int picture(desktop *d,rc_frame *f,char *error,size_t cap) {
    media_status(d);
    if(d->browser.active)rc_browser_draw(&d->browser,d->palette,f);
    else if(d->setup.active)rc_setup_draw(&d->setup,d->palette,f);
    else if(!rc_machine_frame(d->machine,f)) { snprintf(error,cap,"No machine frame");return 0; }
    f->palette=d->palette;
    const size_t n=(size_t)f->width*f->height;
    if(n>d->pixel_count) {
        uint32_t *p=realloc(d->pixels,n*sizeof *p);
        if(!p) { snprintf(error,cap,"Out of memory");return 0; }
        d->pixels=p;d->pixel_count=n;
    }
    rc_frame_argb(f,d->pixels);
    if(message_active(d))rc_tui_status(d->setup.tui.font,d->pixels,f->width,f->height,d->palette,d->osd.text);
    return 1;
}
static int shot(void *ctx,const char *path,char *error,size_t cap) {
    desktop *d=ctx;rc_frame f;
    if(!picture(d,&f,error,cap)) return 0;
    if(!rc_png_write(path,d->pixels,(int)f.width,(int)f.height,(size_t)f.width*4u,0)) {
        snprintf(error,cap,"Cannot write screenshot");return 0;
    }
    return 1;
}
static int state_save(void *ctx,const char *path,char *error,size_t cap) {
    desktop *d=ctx;rc_state_writer w;rc_sw_init(&w);rc_machine_save(d->machine,&w);
    int ok=rc_sw_finish(&w);
    if(!ok) snprintf(error,cap,"Cannot serialise state");
    else ok=rc_host_write_file(path,w.data,w.size,error,cap);
    rc_sw_free(&w);return ok;
}
static int state_load(void *ctx,const char *path,char *error,size_t cap) {
    desktop *d=ctx;uint8_t *data=NULL;size_t size=0;int writable=0;
    if(!rc_host_read_file(path,64u*1024u*1024u,&data,&size,&writable,error,cap)) return 0;
    rc_state_reader r;const int ok=rc_sr_open(&r,data,size,error,cap)&&rc_machine_load(d->machine,&r,error,cap);
    free(data);if(ok){d->pace_dirty=1;rc_typist_cancel(&d->typist);rc_keymap_init(&d->keymap,d->machine,d->config);memset(d->keymap.sent,255,sizeof d->keymap.sent);}return ok;
}
static void release_keys(desktop *d){rc_typist_cancel(&d->typist);rc_keymap_clear(&d->keymap);rc_joykeys_set(&d->joykeys,d->config->keyboard_joystick);const rc_family *f=rc_machine_family(d->machine);for(unsigned i=0;i<f->key_count;++i)rc_machine_key(d->machine,i,0);for(unsigned p=0;p<f->joysticks;++p){rc_machine_joystick(d->machine,p,0);if(d->config->sticks[p].type==RC_CONTROL_MOUSE1351)rc_machine_pointer(d->machine,p,RC_CONTROL_MOUSE1351,0,0,0);}d->suppress_text=0;}
static void input_config(desktop *d){
    release_keys(d);int keys[RC_CONFIG_STICK_KEYS];for(unsigned i=0;i<RC_CONFIG_STICK_KEYS;++i)keys[i]=(int)SDL_GetScancodeFromName(d->config->stick_keys[i]);rc_joykeys_set_keys(&d->joykeys,keys);
    for(unsigned p=0;p<2;++p)rc_machine_pointer(d->machine,p,d->config->sticks[p].type,d->config->sticks[p].type==RC_CONTROL_PADDLES?255:0,d->config->sticks[p].type==RC_CONTROL_PADDLES?255:0,0);
    if(!d->hidden){int mouse=0;for(unsigned p=0;p<2;++p)if(d->config->sticks[p].type==RC_CONTROL_MOUSE1351)mouse=1;SDL_SetRelativeMouseMode(mouse&&!d->setup.active&&!d->browser.active&&!d->paused?SDL_TRUE:SDL_FALSE);}
}
static void input_poll(desktop *d){
    if(d->setup.active||d->browser.active||d->paused)return;
    int explicit_keyboard=0;for(unsigned p=0;p<2;++p)if(d->config->sticks[p].device==RC_STICK_KEYBOARD)explicit_keyboard=1;
    const unsigned keyboard=rc_joykeys_held(&d->joykeys);
    for(unsigned p=0;p<2;++p){const rc_config_stick *st=&d->config->sticks[p];
        if(st->type==RC_CONTROL_JOYSTICK){unsigned held=st->device==RC_STICK_CONTROLLER?rc_pads_joystick(&d->pads,st->pad-1):0;
            if(st->device==RC_STICK_KEYBOARD||(!explicit_keyboard&&p==d->config->keyboard_port))held|=keyboard;
            rc_machine_pointer(d->machine,p,st->type,0,0,0);rc_machine_joystick(d->machine,p,held);
        }else if(st->type==RC_CONTROL_PADDLES){int x=255,y=255;unsigned buttons=0;
            if(st->device==RC_STICK_CONTROLLER)rc_pads_paddles(&d->pads,st->pad-1,&x,&y,&buttons);
            else if(st->device==RC_STICK_MOUSE){int w,h,mx,my;const unsigned held=SDL_GetMouseState(&mx,&my);w=h=0;if(SDL_GetMouseFocus())SDL_GetWindowSize(SDL_GetMouseFocus(),&w,&h);if(w>0)x=mx*255/w;if(h>0)y=my*255/h;if(x<0)x=0;if(x>255)x=255;if(y<0)y=0;if(y>255)y=255;buttons=(held&SDL_BUTTON_LMASK?1u:0)|(held&SDL_BUTTON_RMASK?2u:0);}
            rc_machine_pointer(d->machine,p,st->type,x,y,buttons);
        }else if(st->type==RC_CONTROL_NONE)rc_machine_pointer(d->machine,p,st->type,0,0,0);
    }
}
static void reset(void *ctx,int power) { desktop *d=ctx;release_keys(d);if(power!=2){rc_machine_reset(d->machine,power==1);d->pace_dirty=1;} }
static void speed(void *ctx,int full) { ((desktop *)ctx)->full=full; }
static int audio_capture(void *ctx,const char *path,char *error,size_t cap) {
    desktop *d=ctx;
    if(!rc_wav_close(&d->wav)) { snprintf(error,cap,"Cannot finish audio capture");return 0; }
    if(!strcmp(path,"off")) return 1;
    if(!rc_wav_open(&d->wav,path)) { snprintf(error,cap,"Cannot open audio capture");return 0; }
    return 1;
}
static size_t screen_text(void *ctx,char *out,size_t cap){desktop *d=ctx;size_t n;
    media_status(d);
    if(d->browser.active)n=rc_browser_text(&d->browser,out,cap);
    else if(d->setup.active){rc_frame f;rc_setup_draw(&d->setup,d->palette,&f);n=rc_tui_text(&d->setup.tui,out,cap);}
    else n=rc_machine_text(d->machine,out,cap);
    if(message_active(d)&&n<cap){const int added=snprintf(out+n,cap-n,"%s\n",d->osd.text);if(added>0)n+=(size_t)added<cap-n?(size_t)added:cap-n-1;}
    return n;
}
static int palette_file(void *ctx,const char *name){rc_setup *s=ctx;if(!rc_media_extension_in(name,"vpl"))return 1;
    if(!strcmp(name,"cjam.vpl")||!strcmp(name,"colodore.vpl")||!strcmp(name,"pepto-pal.vpl")||!strcmp(name,"pepto-ntsc.vpl"))return 1;
    rc_setup_palette(s,name);return 1;
}
static int load_palette(desktop *d,char *error,size_t cap){
    const uint32_t *built=rc_palette_builtin(d->config->palette);if(built){memcpy(d->palette,built,sizeof d->palette);return 1;}
    for(unsigned i=0;i<d->palette_folders_count;++i){char path[RC_HOST_PATH];const int n=snprintf(path,sizeof path,"%s/%s",d->palette_folders[i],d->config->palette);if(n<0||(size_t)n>=sizeof path)continue;
        if(!rc_host_file_exists(path))continue;uint8_t *data=NULL;size_t size=0;int writable;
        if(!rc_host_read_file(path,65536,&data,&size,&writable,error,cap))return 0;const int ok=rc_palette_parse(data,size,d->palette,error,cap);free(data);return ok;}
    snprintf(error,cap,"Palette not found: %s",d->config->palette);return 0;
}
static int apply_video(desktop *d,char *error,size_t cap){
    if(!load_palette(d,error,cap))return 0;rc_machine_border(d->machine,d->config->border);
    if(d->video){rc_config *c=d->config;rc_video_set_fullscreen(d->video,c->fullscreen!=0);rc_video_set_filter(d->video,(int)c->filter);
        rc_video_set_look(d->video,c->look==RC_LOOK_CRT?RC_VIDEO_LOOK_CRT:RC_VIDEO_LOOK_PLAIN,RC_VIDEO_PHOSPHOR_COLOR);
        rc_video_set_integer_scale(d->video,c->scale==RC_CONFIG_SCALE_INTEGER?8:0);rc_video_set_aspect(d->video,(int)c->aspect);rc_video_set_vsync(d->video,c->vsync!=0);}
    return 1;
}
static int store_config(desktop *d,char *error,size_t cap){
    if(d->hidden&&!d->config->source[0])return 1;
    const char *path=d->config->source[0]?d->config->source:"rec64.yaml";rc_ydoc doc={0};
    if(rc_host_file_exists(path)&&!rc_ydoc_load(&doc,path)){snprintf(error,cap,"Cannot read configuration");return 0;}
    const int ok=rc_config_store(d->config,&doc)&&rc_ydoc_save(&doc,path);rc_ydoc_free(&doc);if(!ok)snprintf(error,cap,"Cannot save configuration");return ok;
}
static int media_available(const desktop *d){return !strcmp(rc_machine_family(d->machine)->id,"c64");}
static void media_status(desktop *d){
    d->setup.media_available=media_available(d);d->setup.has_tape=d->setup.tape_writable=d->setup.tape_playing=d->setup.tape_recording=0;
    if(!d->setup.media_available)return;rc_media *media=rc_machine_media(d->machine,0);rc_tape_deck *deck=rc_machine_tape_deck(d->machine,0);
    d->setup.has_tape=media!=NULL;d->setup.tape_writable=media&&media->writable&&rc_media_extension_in(media->path,"tap");d->setup.tape_playing=deck&&deck->play;d->setup.tape_recording=deck&&deck->record;
}
static void media_options(desktop *d){const rc_family *f=rc_machine_family(d->machine);for(unsigned i=0;i<f->media_count;++i){rc_config_media effective;rc_config_media_settings(d->config,f->media[i].key,&effective);rc_machine_tape_options(d->machine,i,rc_config_tape_bits(&effective));}}
static int flush_media(desktop *d,char *error,size_t cap){
    const rc_family *f=rc_machine_family(d->machine);
    for(unsigned i=0;i<RC_CONFIG_MEDIA;++i){rc_media *mounted=i<f->media_count?rc_machine_media(d->machine,i):NULL,*owned=d->media[i];
        if(mounted&&!rc_media_save(mounted)){snprintf(error,cap,"%s: %s",mounted->path,mounted->error);return 0;}
        if(owned&&owned!=mounted&&!rc_media_save(owned)){snprintf(error,cap,"%s: %s",owned->path,owned->error);return 0;}}
    return 1;
}
static int mount_media(desktop *d,unsigned slot,const char *path,char *error,size_t cap){
    const rc_family *f=rc_machine_family(d->machine);if(slot>=f->media_count||slot>=RC_CONFIG_MEDIA){snprintf(error,cap,"No media slot available");return 0;}
    char absolute[RC_MEDIA_PATH];if(!rc_host_full_path(path,absolute,sizeof absolute)){snprintf(error,cap,"Path is too long");return 0;}
    rc_config_media effective;rc_config_media_settings(d->config,f->media[slot].key,&effective);
    rc_media *next=rc_media_open(absolute,effective.write_protect,error,cap);if(!next)return 0;
    rc_media *old=rc_machine_media(d->machine,slot);if(old&&!rc_media_save(old)){snprintf(error,cap,"%s",old->error);rc_media_close(next);return 0;}
    if(!rc_machine_insert(d->machine,slot,next,error,cap)){rc_media_close(next);return 0;}
    rc_media_close(d->media[slot]);d->media[slot]=next;d->media_entry[slot]=UINT32_MAX;
    rc_config_media *setting=rc_config_media_slot(d->config,f->media[slot].key);if(setting)snprintf(setting->image,sizeof setting->image,"%s",absolute);
    if(slot!=2)d->source_slot=(int)slot;media_options(d);media_status(d);return store_config(d,error,cap);
}
static int insert(void *ctx,unsigned slot,const char *path,char *error,size_t cap){desktop *d=ctx;release_keys(d);return mount_media(d,slot,path,error,cap);}
static int eject(void *ctx,unsigned slot){
    desktop *d=ctx;const rc_family *f=rc_machine_family(d->machine);if(slot>=f->media_count||slot>=RC_CONFIG_MEDIA)return 0;
    rc_media *mounted=rc_machine_media(d->machine,slot);if(!mounted)return 0;if(!rc_media_save(mounted)){message(d,mounted->error);return 0;}
    if(!rc_machine_eject(d->machine,slot))return 0;if(d->source_slot==(int)slot)d->source_slot=-1;release_keys(d);rc_media_close(d->media[slot]);d->media[slot]=NULL;d->media_entry[slot]=UINT32_MAX;
    rc_config_media *setting=rc_config_media_slot(d->config,f->media[slot].key);if(setting)setting->image[0]=0;char error[128];if(!store_config(d,error,sizeof error))message(d,error);media_status(d);return 1;
}
static int autoload(desktop *d,unsigned slot,char *error,size_t cap){
    const rc_family *f=rc_machine_family(d->machine);if(slot>=f->media_count)return 0;
    rc_config_media effective;rc_config_media_settings(d->config,f->media[slot].key,&effective);if(!effective.autoload)return 1;
    int power=0;const char *commands=rc_machine_autoload(d->machine,slot,&power);if(!commands||!*commands)return 1;
    char text[512];snprintf(text,sizeof text,"%s",commands);release_keys(d);if(power)rc_machine_reset(d->machine,1);d->pace_dirty=1;
    return rc_typist_start(&d->typist,text,error,cap);
}
static int tape_directory(void *ctx,const rc_media *image,rc_browser_content *out,size_t capacity,size_t *count,char *error,size_t cap){
    (void)ctx;rc_tap tape={0};if(!rc_tap_open(&tape,image->data,image->size,error,cap))return 0;
    size_t total=0;int ok=rc_tape_directory(&tape,NULL,0,&total,error,cap);rc_tape_entry *entries=NULL;
    if(ok&&capacity&&total){entries=calloc(total,sizeof *entries);if(!entries){snprintf(error,cap,"Out of memory");ok=0;}else ok=rc_tape_directory(&tape,entries,total,&total,error,cap);}
    if(ok){*count=total;for(size_t i=0;i<total&&i<capacity;++i){memcpy(out[i].name,entries[i].name,16);out[i].address=entries[i].address;out[i].length=entries[i].length;out[i].index=(uint32_t)i;out[i].type=0x82;}}
    free(entries);rc_tap_close(&tape);return ok;
}
static int image_slot(const desktop *d,const char *path){const rc_family *f=rc_machine_family(d->machine);for(unsigned i=0;i<f->media_count;++i)if(rc_media_extension_in(path,f->media[i].extensions))return (int)i;return -1;}
static void close_browser(desktop *d){rc_browser_close(&d->browser);input_config(d);d->pace_dirty=1;}
static int browser_open(desktop *d,int save,char *error,size_t cap){
    if(!media_available(d)){snprintf(error,cap,"No media device available");return 0;}char folder[RC_MEDIA_PATH]=".";
    if(d->browser.path[0])snprintf(folder,sizeof folder,"%s",d->browser.path);
    else if(d->config->source[0]){snprintf(folder,sizeof folder,"%s",d->config->source);char *slash=strrchr(folder,'/'),*back=strrchr(folder,'\\');if(back&&(!slash||back>slash))slash=back;if(slash)slash[1]=0;else snprintf(folder,sizeof folder,".");}
    if(!rc_browser_open(&d->browser,folder,save,error,cap))return 0;release_keys(d);input_config(d);d->pace_dirty=1;return 1;
}
static int save_program(desktop *d,const char *path,char *error,size_t cap){
    uint8_t pointers[4];for(unsigned i=0;i<4;++i)if(!rc_machine_peek(d->machine,0x2b+i,&pointers[i]))return 0;
    const unsigned start=pointers[0]|(unsigned)pointers[1]<<8,end=pointers[2]|(unsigned)pointers[3]<<8;
    if(start<0x200||end<=start){snprintf(error,cap,"No valid BASIC program in memory");return 0;}
    const size_t bytes=end-start;uint8_t *data=malloc(bytes+2);if(!data){snprintf(error,cap,"Out of memory");return 0;}data[0]=(uint8_t)start;data[1]=(uint8_t)(start>>8);
    int ok=1;for(size_t i=0;i<bytes;++i)if(!rc_machine_peek(d->machine,start+(uint32_t)i,&data[i+2])){ok=0;snprintf(error,cap,"Cannot read program memory");break;}
    if(ok)ok=rc_host_write_file(path,data,bytes+2,error,cap);free(data);return ok;
}
static int browser_action(desktop *d,unsigned action,char *error,size_t cap){
    if(!action)return 1;if(action==RC_BROWSER_CLOSE){close_browser(d);return 1;}
    const rc_browser_result selected=d->browser.result;int slot=-1,load=0;
    if(action==RC_BROWSER_LOAD||action==RC_BROWSER_INSERT){slot=image_slot(d,selected.path);if(slot<0){snprintf(error,cap,"No device for this image");return 0;}
        if(!mount_media(d,(unsigned)slot,selected.path,error,cap))return 0;
        if(selected.entry!=UINT32_MAX&&!rc_machine_control(d->machine,(unsigned)slot,RC_MEDIA_SEEK,selected.entry)){snprintf(error,cap,"Cannot select image entry");return 0;}
        d->media_entry[slot]=selected.entry;if(selected.entry!=UINT32_MAX)d->source_slot=slot;load=action==RC_BROWSER_LOAD;
    }else {
        if(rc_host_file_exists(selected.path)&&!selected.overwrite){snprintf(error,cap,"File exists: confirm replacement");return 0;}
        if(!flush_media(d,error,cap))return 0;
        if(action==RC_BROWSER_SAVE_PRG){if(!save_program(d,selected.path,error,cap))return 0;}
        else if(action==RC_BROWSER_SAVE_TAP){rc_tap tape={0};if(!rc_tap_create(&tape,1,error,cap))return 0;const int ok=rc_host_write_file(selected.path,tape.image->data,tape.image->size,error,cap);rc_tap_close(&tape);if(!ok)return 0;slot=image_slot(d,selected.path);if(slot<0||!mount_media(d,(unsigned)slot,selected.path,error,cap))return 0;if(!rc_machine_control(d->machine,(unsigned)slot,RC_MEDIA_RECORD,0)){snprintf(error,cap,"Cannot record this tape");return 0;}}
        else if(action==RC_BROWSER_SAVE_D64){rc_d64 disk={0};if(!rc_d64_create(&disk,NULL,NULL,error,cap))return 0;const int ok=rc_host_write_file(selected.path,disk.image->data,disk.image->size,error,cap);rc_d64_close(&disk);if(!ok)return 0;slot=image_slot(d,selected.path);if(slot<0||!mount_media(d,(unsigned)slot,selected.path,error,cap))return 0;}
    }
    close_browser(d);d->setup.active=0;input_config(d);
    if(load&&!autoload(d,(unsigned)slot,error,cap))return 0;
    message(d,action>=RC_BROWSER_SAVE_PRG?"Saved":"Image inserted");return 1;
}
static int drop(void *ctx,const char *path,char *error,size_t cap){
    desktop *d=ctx;if(!media_available(d)){snprintf(error,cap,"No media device available");return 0;}
    const unsigned action=rc_browser_file(&d->browser,path,error,cap);if(cap&&error[0])return 0;
    if(action)return browser_action(d,action,error,cap);release_keys(d);input_config(d);d->pace_dirty=1;return 1;
}
static int text_input(void *ctx,const char *text){
    desktop *d=ctx;if(!d->browser.active){if(rc_typist_busy(&d->typist))release_keys(d);return 0;}
    for(;*text&&d->browser.active;++text){char error[128]="";unsigned action=0;
        if(*text=='\n'||*text=='\r')action=rc_browser_key(&d->browser,"RETURN");
        else if(*text=='\b')action=rc_browser_key(&d->browser,"BACKSPACE");
        else if(*text=='\t')action=rc_browser_key(&d->browser,"TAB");
        else {const char ch[2]={*text,0};rc_browser_input(&d->browser,ch);}
        if(action&&!browser_action(d,action,error,sizeof error))rc_browser_error(&d->browser,error);
    }return 1;
}
static int automatic_speed(desktop *d){
    const unsigned active=rc_machine_activity(d->machine);const rc_family *f=rc_machine_family(d->machine);
    for(unsigned i=0;i<f->media_count;++i)if(active&(1u<<i)){rc_config_media effective;rc_config_media_settings(d->config,f->media[i].key,&effective);if(effective.fast&&effective.full_speed)return 1;}return 0;
}
static int restart(desktop *d,char *error,size_t cap){
    if(!flush_media(d,error,cap))return 0;rc_machine *next=rc_machine_create(d->config,d->roms,error,cap);if(!next)return 0;
    if(strcmp(d->config->model,"test")&&strcmp(rc_machine_family(next)->id,"c64")){rc_machine_destroy(next);snprintf(error,cap,"Required firmware is missing");return 0;}
    rc_media *clones[RC_CONFIG_MEDIA]={0};const rc_family *family=rc_machine_family(next),*old_family=rc_machine_family(d->machine);int ok=1;
    for(unsigned i=0;i<family->media_count&&i<RC_CONFIG_MEDIA;++i){rc_config_media effective;rc_config_media_settings(d->config,family->media[i].key,&effective);rc_machine_tape_options(next,i,rc_config_tape_bits(&effective));
        rc_media *mounted=i<old_family->media_count?rc_machine_media(d->machine,i):d->media[i];
        if(mounted&&mounted!=d->media[i]){clones[i]=rc_media_from(mounted->path,mounted->data,mounted->size);if(!clones[i]){snprintf(error,cap,"Out of memory");ok=0;break;}clones[i]->writable=mounted->writable;clones[i]->dirty=mounted->dirty;mounted=clones[i];}
        if(mounted&&(!rc_machine_insert(next,i,mounted,error,cap)||(d->media_entry[i]!=UINT32_MAX&&!rc_machine_control(next,i,RC_MEDIA_SEEK,d->media_entry[i])))){ok=0;break;}}
    if(ok&&d->source_slot>=0&&(unsigned)d->source_slot<family->media_count&&rc_machine_media(next,(unsigned)d->source_slot))ok=rc_machine_control(next,(unsigned)d->source_slot,RC_MEDIA_SEEK,d->media_entry[d->source_slot]);
    if(!ok){rc_machine_destroy(next);for(unsigned i=0;i<RC_CONFIG_MEDIA;++i)rc_media_close(clones[i]);return 0;}
    release_keys(d);rc_machine_destroy(d->machine);*d->owner=d->machine=next;
    for(unsigned i=0;i<RC_CONFIG_MEDIA;++i)if(clones[i]){rc_media_close(d->media[i]);d->media[i]=clones[i];}
    rc_keymap_init(&d->keymap,next,d->config);rc_machine_border(next,d->config->border);d->running_config=*d->config;d->pace_dirty=1;media_status(d);return 1;
}

static void restore_machine_config(desktop *d){snprintf(d->config->model,sizeof d->config->model,"%s",d->running_config.model);d->config->option_count=d->running_config.option_count;memcpy(d->config->options,d->running_config.options,sizeof d->config->options);}
static int needs_restart(desktop *d){
    unsigned model=0,values[RC_CONFIG_OPTIONS];const rc_family *f=rc_family_of_model(d->config->model,&model);
    if(f!=rc_machine_family(d->machine)||model!=rc_machine_model(d->machine))return 1;
    rc_family_resolve(f,model,d->config,values,NULL);const unsigned *current=rc_machine_options(d->machine);
    for(unsigned i=0;i<f->option_count;++i)if((f->options[i].flags&RC_OPTION_RESET)&&values[i]!=current[i])return 1;
    return 0;
}
/* The same host controls serve SDL and the headless harness. */
static int shell_key(void *ctx,const char *name,int down){
    desktop *d=ctx;char key[96];size_t n=strlen(name);if(n>=sizeof key)return 0;
    for(size_t i=0;i<=n;++i)key[i]=(char)toupper((unsigned char)name[i]);
    const char *bare=strrchr(key,'+');bare=bare?bare+1:key;
    const int ctrl=strstr(key,"CTRL+")!=NULL,alt=strstr(key,"ALT+")!=NULL,shifted=strstr(key,"SHIFT+")!=NULL;
    char error[256]="";
    /* This remains a host control while a setup row has the focus. */
    if(alt&&(!strcmp(bare,"RETURN")||!strcmp(bare,"ENTER")||!strcmp(bare,"KEYPAD ENTER"))){
        if(down){d->config->fullscreen=!d->config->fullscreen;
            if(!apply_video(d,error,sizeof error)||!store_config(d,error,sizeof error))message(d,error);}
        return 1;
    }
    if(ctrl&&(!strcmp(bare,"F1")||!strcmp(bare,"F2"))){if(down&&!browser_open(d,!strcmp(bare,"F2"),error,sizeof error))message(d,error);return 1;}
    if(alt&&!strcmp(bare,"T")){if(!media_available(d)){if(down)message(d,"No tape available");return 1;}if(down){rc_config_media *tape=rc_config_media_slot(d->config,"tape");if(tape){tape->sound=!tape->sound;tape->explicit_flags|=RC_MEDIA_SETTING_SOUND;media_options(d);message(d,tape->sound?"Tape sound on":"Tape sound off");if(!store_config(d,error,sizeof error))message(d,error);}}return 1;}
    if(d->browser.active&&strcmp(bare,"F12")){
        if(!down)return 1;const int confirm=d->browser.confirm;const unsigned action=rc_browser_key(&d->browser,ctrl?key:bare);
        const int suppress=confirm||action!=RC_BROWSER_NONE||(ctrl&&!alt);
        if(action&&!browser_action(d,action,error,sizeof error))rc_browser_error(&d->browser,error);d->suppress_text=suppress;return 1;
    }
    if(!strcmp(bare,"F12")){
        if(!down)return 1;
        if(alt||ctrl){reset(d,ctrl?1:0);message(d,ctrl?"Cold boot":"Reset");return 1;}
        if(shifted){d->quit=1;return 2;}
        if(d->browser.active){close_browser(d);return 1;}
        if(!d->setup.active){release_keys(d);d->setup.active=1;d->setup.row=0;}
        else {d->setup.active=0;if(d->restart_pending){if(!restart(d,error,sizeof error)){restore_machine_config(d);message(d,error);}d->restart_pending=0;}if(!store_config(d,error,sizeof error))message(d,error);}
        d->pace_dirty=1;input_config(d);return 1;
    }
    if(d->setup.active){
        if(!down)return 1;media_status(d);const rc_config before=*d->config;const unsigned action=rc_setup_key(&d->setup,bare);
        if(action&(RC_SETUP_LOAD|RC_SETUP_SAVE)){if(!browser_open(d,(action&RC_SETUP_SAVE)!=0,error,sizeof error))message(d,error);}
        if(action&RC_SETUP_MEDIA_OPTIONS)media_options(d);
        if(action&RC_SETUP_EJECT){if(!eject(d,0))message(d,"Cannot eject tape");}
        if(action&(RC_SETUP_PLAY|RC_SETUP_STOP|RC_SETUP_REWIND|RC_SETUP_RECORD)){const unsigned command=action&RC_SETUP_PLAY?RC_MEDIA_PLAY:action&RC_SETUP_STOP?RC_MEDIA_STOP:action&RC_SETUP_REWIND?RC_MEDIA_REWIND:RC_MEDIA_RECORD;if(!rc_machine_control(d->machine,0,command,0))message(d,"Tape control unavailable");media_status(d);}
        if(action&RC_SETUP_INPUT)input_config(d);
        if(action&RC_SETUP_VIDEO){if(!apply_video(d,error,sizeof error)){*d->config=before;message(d,error);}}
        if(action&RC_SETUP_RESTART)d->restart_pending=needs_restart(d);
        if(action&RC_SETUP_AUDIO){if(d->config->sound&&!d->hidden&&!d->audio.device)rc_host_audio_open(&d->audio);}
        if(action&(RC_SETUP_RESET|RC_SETUP_COLD)){if(d->restart_pending){if(!restart(d,error,sizeof error)){restore_machine_config(d);message(d,error);}d->restart_pending=0;}reset(d,(action&RC_SETUP_COLD)!=0);message(d,action&RC_SETUP_COLD?"Cold boot":"Reset");}
        if(action&RC_SETUP_OFF){d->quit=1;if(!store_config(d,error,sizeof error))message(d,error);}
        if(action&RC_SETUP_CLOSE){if(d->restart_pending){if(!restart(d,error,sizeof error)){restore_machine_config(d);message(d,error);}d->restart_pending=0;}d->pace_dirty=1;input_config(d);}
        if(action&&!(action&RC_SETUP_OFF)&&!d->restart_pending&&!store_config(d,error,sizeof error))message(d,error);
        return action&RC_SETUP_OFF?2:1;
    }
    if(!strcmp(bare,"F11")){
        if(down){release_keys(d);if(shifted){const rc_config_stick temp=d->config->sticks[0];d->config->sticks[0]=d->config->sticks[1];d->config->sticks[1]=temp;d->config->keyboard_port^=1u;message(d,"Control ports swapped");}
            else {d->config->keyboard_joystick=!d->config->keyboard_joystick;message(d,d->config->keyboard_joystick?"Keyboard joystick on":"Keyboard joystick off");}
            input_config(d);if(!store_config(d,error,sizeof error))message(d,error);
        }return 1;
    }
    if(!strcmp(bare,"F10")){if(down){d->full=!d->full;d->pace_dirty=1;message(d,d->full?"Full speed (sound muted)":"100% speed");}return 1;}
    if(!strcmp(bare,"PAUSE")){if(down){d->paused=!d->paused;d->pace_dirty=1;release_keys(d);message(d,d->paused?"Paused":"Running");input_config(d);}return 1;}
    if(ctrl&&(!strcmp(bare,"F5")||!strcmp(bare,"F6"))){if(down)message(d,"Snapshot slots unavailable");return 1;}
    if((alt&&(!strcmp(bare,"F")||!strcmp(bare,"S")))||!strcmp(bare,"F9")){if(down)message(d,"Control unavailable");return 1;}
    if(!strcmp(bare,"PRINTSCREEN")||!strcmp(bare,"PRINT SCREEN")){
        if(down){char path[RC_HOST_PATH];rc_host_make_folders("work/screenshots");snprintf(path,sizeof path,"work/screenshots/rec64-%llu.png",(unsigned long long)d->frames);if(shot(d,path,error,sizeof error))message(d,path);else message(d,error);}return 1;
    }
    /* Headless host-key commands use the same configurable physical map. */
    if(down&&rc_typist_busy(&d->typist))release_keys(d);
    const SDL_Scancode sc=SDL_GetScancodeFromName(name);
    if(sc!=SDL_SCANCODE_UNKNOWN){if(rc_joykeys_key(&d->joykeys,(int)sc,down)){for(unsigned p=0;p<2;++p)if(d->config->sticks[p].type==RC_CONTROL_JOYSTICK&&(p==d->config->keyboard_port||d->config->sticks[p].device==RC_STICK_KEYBOARD))rc_machine_joystick(d->machine,p,rc_joykeys_held(&d->joykeys));return 1;}
        return rc_keymap_key(&d->keymap,(unsigned)sc,name,down);}
    return 0;
}
static int control_read(void *ctx,char *line,size_t cap) {
    desktop *d=ctx;
    if(d->stdin_control) {
        if(!fgets(line,(int)cap,stdin)) return -1;
        if(!strchr(line,'\n')&&!feof(stdin)) { d->io_failed=1;return -1; }
        return 1;
    }
    for(;;) {
        uint8_t c;const long n=rc_host_stream_read(d->tcp,&c,1);
        if(n<0) { d->input_size=0;return 0; }
        if(!n) return 0;
        if(c=='\n') {
            d->input[d->input_size]=0;snprintf(line,cap,"%s",d->input);d->input_size=0;return 1;
        }
        if(d->input_size+1>=sizeof d->input) { d->io_failed=1;return -1; }
        if(c!='\r') d->input[d->input_size++]=(char)c;
    }
}
static void control_write(void *ctx,const char *line) {
    desktop *d=ctx;
    if(d->stdin_control) { puts(line);fflush(stdout);return; }
    char reply[17000];const int n=snprintf(reply,sizeof reply,"%s\n",line);
    if(n<0||(size_t)n>=sizeof reply) { d->io_failed=1;return; }
    size_t sent=0;
    for(unsigned tries=0;sent<(size_t)n&&tries<1000;++tries) {
        const long wrote=rc_host_stream_write(d->tcp,(const uint8_t *)reply+sent,(size_t)n-sent);
        if(wrote<0) { d->io_failed=1;break; }
        sent+=(size_t)wrote;if(!wrote) rc_host_sleep_us(1000);
    }
    if(sent!=(size_t)n) d->io_failed=1;
}
int rc_desktop_run(rc_machine **owner,rc_config *c,const rc_rom_index *roms,const rc_desktop_options *o,char *error,size_t cap) {
    rc_machine *m=*owner;desktop d={0};d.machine=m;d.owner=owner;d.config=c;d.roms=roms;d.hidden=o->hidden;d.running_config=*c;d.source_slot=-1;for(unsigned i=0;i<RC_CONFIG_MEDIA;++i)d.media_entry[i]=UINT32_MAX;rc_browser_init(&d.browser);rc_browser_set_tape_reader(&d.browser,tape_directory,&d);rc_typist_init(&d.typist);
    rc_keymap_init(&d.keymap,m,c);int keys[RC_CONFIG_STICK_KEYS];for(unsigned i=0;i<RC_CONFIG_STICK_KEYS;++i)keys[i]=(int)SDL_GetScancodeFromName(c->stick_keys[i]);rc_joykeys_init(&d.joykeys,keys,c->keyboard_joystick);
    int status=1;rc_harness *h=NULL;rc_setup_init(&d.setup,c);
    char folder[RC_HOST_PATH];
    if(c->source[0]){snprintf(folder,sizeof folder,"%s",c->source);char *slash=strrchr(folder,'/'),*back=strrchr(folder,'\\');if(back&&(!slash||back>slash))slash=back;if(slash)slash[1]=0;else folder[0]=0;snprintf(d.palette_folders[d.palette_folders_count++],RC_HOST_PATH,"%spalettes",folder);}
    snprintf(d.palette_folders[d.palette_folders_count++],RC_HOST_PATH,"palettes");
    if(rc_host_program_folder(folder,sizeof folder))snprintf(d.palette_folders[d.palette_folders_count++],RC_HOST_PATH,"%spalettes",folder);
    for(unsigned i=0;i<d.palette_folders_count;++i)rc_host_list_files(d.palette_folders[i],palette_file,&d.setup);
    if(!apply_video(&d,error,cap))return 1;
    if(strcmp(c->model,"test")&&!strcmp(rc_machine_family(m)->id,"stub")){d.setup.active=1;message(&d,"Required firmware missing: use roms/");}
    const int driven=o->script||o->control;
    if(SDL_Init(o->hidden?SDL_INIT_TIMER:SDL_INIT_VIDEO|SDL_INIT_AUDIO|SDL_INIT_TIMER)!=0) {
        snprintf(error,cap,"SDL: %s",SDL_GetError());return 1;
    }
    if(!o->hidden) {
        rc_pads_open(&d.pads);SDL_StartTextInput();
        const rc_settings settings={c->fullscreen!=0,2,c->scale==RC_CONFIG_SCALE_INTEGER?8:0,(int)c->aspect,
            (int)c->filter,c->look==RC_LOOK_CRT,c->vsync!=0};
        rc_frame f;if(!picture(&d,&f,error,cap)) goto done;
        d.video=rc_video_open("ReC64",&settings,(int)f.width,(int)f.height,(int)f.aspect_w,(int)f.aspect_h,false,error,cap);
        if(!d.video) goto done;
        d.setup.crt_available=rc_video_crt_available(d.video);
        if(!d.setup.crt_available&&c->look==RC_LOOK_CRT){c->look=RC_LOOK_PLAIN;c->filter=RC_CONFIG_FILTER_NEAREST;message(&d,"CRT unavailable: nearest filtering");}
        if(!apply_video(&d,error,cap))goto done;
        if(c->sound) rc_host_audio_open(&d.audio);
    }
    input_config(&d);
    int startup=-1;const rc_family *family=rc_machine_family(d.machine);media_options(&d);
    for(unsigned i=0;i<family->media_count;++i){rc_config_media settings;rc_config_media_settings(c,family->media[i].key,&settings);if(settings.image[0]){if(!mount_media(&d,i,settings.image,error,cap))goto done;if(settings.autoload){if(i!=2||rc_machine_control(d.machine,i,RC_MEDIA_SEEK,0)){startup=(int)i;if(i==2){d.media_entry[i]=0;d.source_slot=2;}}}}}
    if(startup>=0&&!autoload(&d,(unsigned)startup,error,cap))goto done;
    if(o->audio&&!audio_capture(&d,o->audio,error,cap)) goto done;
    if(o->control) {
        if(!strcmp(o->control,"stdin")) d.stdin_control=1;
        else {
            d.tcp=rc_host_stream_open(RC_STREAM_TCP_LISTEN,o->control+4,error,cap);
            if(!d.tcp) goto done;
        }
    }
    if(driven) {
        rc_harness_host host={0};host.ctx=&d;host.machine=machine;host.shot=shot;
        host.state_save=state_save;host.state_load=state_load;host.reset=reset;host.speed=speed;
        host.audio=audio_capture;host.deterministic=o->hidden;
        host.key=shell_key;host.screen_text=screen_text;host.text_input=text_input;host.insert=insert;host.eject=eject;host.drop=drop;
        if(o->control) { host.control_read=control_read;host.control_write=control_write; }
        h=rc_harness_open(&host,o->script,error,cap);if(!h) goto done;
    }
    rc_pace pace;rc_pace_init(&pace,SDL_GetPerformanceFrequency(),rc_machine_clock(m),100);
    rc_pace_reset(&pace,SDL_GetPerformanceCounter(),rc_machine_time(m));
    for(;;) {
        if(d.quit||(o->frames&&d.frames>=o->frames)) { status=0;break; }
        if(!o->hidden) {
            SDL_Event e;
            while(SDL_PollEvent(&e)) {
                if(e.type==SDL_QUIT) { status=0;goto done; }
                rc_pad_key pad=RC_PAD_NONE;const rc_pads_result change=rc_pads_event(&d.pads,&e,&pad);
                if(change==RC_PADS_CHANGED){input_poll(&d);message(&d,"Controller connections changed");}
                if(change==RC_PADS_KEY&&(d.setup.active||d.browser.active||pad==RC_PAD_START)){
                    const char *names[]={"","UP","DOWN","LEFT","RIGHT","RETURN","ESCAPE","-","+","F12",""};if(pad!=RC_PAD_NONE&&pad!=RC_PAD_BACK)shell_key(&d,names[pad],1);}
                if(e.type==SDL_WINDOWEVENT&&e.window.event==SDL_WINDOWEVENT_FOCUS_LOST)release_keys(&d);
                else if(e.type==SDL_DROPFILE){if(!drop(&d,e.drop.file,error,cap))message(&d,error);SDL_free(e.drop.file);}
                else if(e.type==SDL_TEXTINPUT){if(d.browser.active){if(!d.suppress_text)rc_browser_input(&d.browser,e.text.text);}
                    else if(!d.setup.active&&!d.paused&&!d.suppress_text){rc_typist_cancel(&d.typist);if(!rc_keymap_text(&d.keymap,e.text.text))message(&d,"Character unavailable on C64 keyboard");}}
                else if(e.type==SDL_MOUSEMOTION||e.type==SDL_MOUSEBUTTONDOWN||e.type==SDL_MOUSEBUTTONUP){
                    if(!d.setup.active&&!d.browser.active&&!d.paused){const unsigned held=SDL_GetMouseState(NULL,NULL),buttons=(held&SDL_BUTTON_LMASK?1u:0)|(held&SDL_BUTTON_RMASK?2u:0);
                        for(unsigned p=0;p<2;++p)if(c->sticks[p].type==RC_CONTROL_MOUSE1351)rc_machine_pointer(d.machine,p,RC_CONTROL_MOUSE1351,e.type==SDL_MOUSEMOTION?e.motion.xrel:0,e.type==SDL_MOUSEMOTION?e.motion.yrel:0,buttons);}}
                else if(e.type==SDL_KEYDOWN||e.type==SDL_KEYUP){
                    if(e.key.repeat&&(!(d.setup.active||d.browser.active)||(e.key.keysym.mod&(KMOD_ALT|KMOD_CTRL))||e.key.keysym.scancode==SDL_SCANCODE_F12))continue;
                    const int down=e.type==SDL_KEYDOWN;const char *name=SDL_GetScancodeName(e.key.keysym.scancode);
                    char hostkey[96];snprintf(hostkey,sizeof hostkey,"%s%s%s%s",e.key.keysym.mod&KMOD_CTRL?"Ctrl+":"",e.key.keysym.mod&KMOD_ALT?"Alt+":"",e.key.keysym.mod&KMOD_SHIFT?"Shift+":"",name);
                    /* Always release the original binding, even if modifiers changed. */
                    if(!down){rc_joykeys_key(&d.joykeys,(int)e.key.keysym.scancode,0);rc_keymap_key(&d.keymap,(unsigned)e.key.keysym.scancode,name,0);}
                    const int browser_was_active=d.browser.active;const int handled=shell_key(&d,hostkey,down);
                    if(down&&!browser_was_active)d.suppress_text=handled!=0;
                    if(handled||d.paused)continue;if(down)rc_typist_cancel(&d.typist);
                    if(rc_joykeys_key(&d.joykeys,(int)e.key.keysym.scancode,down)){d.suppress_text=down;continue;}
                    const int mapped=rc_keymap_key(&d.keymap,(unsigned)e.key.keysym.scancode,name,down);
                    if(down)d.suppress_text=mapped;
                }

            }
        }
        if(!o->hidden){rc_pad_key pad;if((d.setup.active||d.browser.active)&&rc_pads_repeat(&d.pads,SDL_GetTicks(),&pad)){const char *names[]={"","UP","DOWN","LEFT","RIGHT"};shell_key(&d,names[pad],1);}input_poll(&d);}
        m=d.machine;
        if(d.pace_dirty){rc_pace_init(&pace,SDL_GetPerformanceFrequency(),rc_machine_clock(m),100);rc_pace_reset(&pace,SDL_GetPerformanceCounter(),rc_machine_time(m));d.pace_dirty=0;}
        uint64_t ticks=rc_machine_frame_period(m);int present=1;
        if(h) {
            const int action=rc_harness_step(h);
            if(action==RC_HARNESS_QUIT) { status=rc_harness_status(h);if(status) snprintf(error,cap,"%s",rc_harness_error(h));break; }
            if(action==RC_HARNESS_IDLE) { rc_host_sleep_us(1000);continue; }
        } else if(!o->hidden&&(d.setup.active||d.browser.active||d.paused)) {rc_host_sleep_us(16000);d.pace_dirty=1;}
        else if(!o->hidden) {
            const rc_pace_step step=rc_pace_next(&pace,d.full||automatic_speed(&d),SDL_GetPerformanceCounter(),rc_machine_time(m),ticks);
            if(step.action==RC_PACE_WAIT) { rc_host_sleep_us(500);continue; }
            present=step.action==RC_PACE_PRESENT;ticks=present?0:step.ticks;
        }
        if(d.quit){status=0;break;}m=d.machine;
        if(!d.setup.active&&!d.browser.active&&!d.paused&&present&&rc_typist_frame(&d.typist,m,&d.keymap)<0)message(&d,d.typist.error);
        if(!d.setup.active&&!d.browser.active&&!d.paused&&rc_machine_run(m,ticks)!=RC_RUN_OK) { snprintf(error,cap,"%s",rc_machine_error(m));goto done; }
        if(present){++d.frames;if(!d.setup.active&&!d.browser.active&&!d.paused)rc_keymap_frame(&d.keymap);}
        if(d.wav.file) rc_wav_pump(&d.wav,m);
        else rc_host_audio_pump(&d.audio,m,(unsigned)c->volume,o->hidden||d.full||!c->sound||d.setup.active||d.browser.active||d.paused);
        if(d.video&&present) {
            rc_frame f;if(!picture(&d,&f,error,cap)) goto done;
            rc_video_set_geometry(d.video,(int)f.width,(int)f.height,(int)f.aspect_w,(int)f.aspect_h);
            rc_video_present(d.video,d.pixels,(int)f.width,(int)f.height);
        }
        if(d.io_failed) { snprintf(error,cap,"Control channel failed");goto done; }
    }
    if(!status&&o->screenshot&&!shot(&d,o->screenshot,error,cap)) status=1;
done:
    if(d.io_failed) { snprintf(error,cap,"Control channel failed");status=1; }
    if(!flush_media(&d,error,cap))status=1;
    if(!rc_wav_close(&d.wav)) { snprintf(error,cap,"Cannot finish audio capture");status=1; }
    rc_harness_close(h);rc_host_stream_close(d.tcp);rc_host_audio_close(&d.audio);
    if(!o->hidden){SDL_StopTextInput();rc_pads_close(&d.pads);}rc_video_close(d.video);rc_browser_close(&d.browser);const rc_family *final_family=rc_machine_family(d.machine);for(unsigned i=0;i<RC_CONFIG_MEDIA;++i){if(i<final_family->media_count&&rc_machine_media(d.machine,i))rc_machine_eject(d.machine,i);rc_media_close(d.media[i]);}free(d.pixels);SDL_Quit();return status;
}
