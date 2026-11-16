/* The host loop. Headless runs never initialise SDL video or create a window. */
#include "desktop.h"
#include "harness.h"
#include "pace.h"
#include "setup.h"
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
    rc_setup setup;rc_osd osd;uint32_t palette[16];
    char palette_folders[3][RC_HOST_PATH];unsigned palette_folders_count;
    int hidden,paused,quit,restart_pending,pace_dirty;uint64_t frames;rc_config running_config;
    uint32_t *pixels;
    size_t pixel_count;
    char input[1024];
    size_t input_size;
    int stdin_control, full, io_failed;
} desktop;
static rc_machine *machine(void *ctx) { return ((desktop *)ctx)->machine; }
static int message_active(const desktop *d){return d->osd.text[0]&&(int32_t)(d->osd.until_ms-(uint32_t)(d->frames*20u))>=0;}
static void message(desktop *d,const char *text){rc_osd_show(&d->osd,(uint32_t)(d->frames*20u),"%s",text);}
static int picture(desktop *d,rc_frame *f,char *error,size_t cap) {
    if(d->setup.active)rc_setup_draw(&d->setup,d->palette,f);
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
    free(data);if(ok){d->pace_dirty=1;memset(d->keymap.sent,255,sizeof d->keymap.sent);}return ok;
}
static void release_keys(desktop *d){rc_keymap_clear(&d->keymap);rc_joykeys_set(&d->joykeys,d->config->keyboard_joystick);const rc_family *f=rc_machine_family(d->machine);for(unsigned i=0;i<f->key_count;++i)rc_machine_key(d->machine,i,0);for(unsigned p=0;p<f->joysticks;++p){rc_machine_joystick(d->machine,p,0);if(d->config->sticks[p].type==RC_CONTROL_MOUSE1351)rc_machine_pointer(d->machine,p,RC_CONTROL_MOUSE1351,0,0,0);}d->suppress_text=0;}
static void input_config(desktop *d){
    release_keys(d);int keys[RC_CONFIG_STICK_KEYS];for(unsigned i=0;i<RC_CONFIG_STICK_KEYS;++i)keys[i]=(int)SDL_GetScancodeFromName(d->config->stick_keys[i]);rc_joykeys_set_keys(&d->joykeys,keys);
    for(unsigned p=0;p<2;++p)rc_machine_pointer(d->machine,p,d->config->sticks[p].type,d->config->sticks[p].type==RC_CONTROL_PADDLES?255:0,d->config->sticks[p].type==RC_CONTROL_PADDLES?255:0,0);
    if(!d->hidden){int mouse=0;for(unsigned p=0;p<2;++p)if(d->config->sticks[p].type==RC_CONTROL_MOUSE1351)mouse=1;SDL_SetRelativeMouseMode(mouse&&!d->setup.active&&!d->paused?SDL_TRUE:SDL_FALSE);}
}
static void input_poll(desktop *d){
    if(d->setup.active||d->paused)return;
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
    if(d->setup.active){rc_frame f;rc_setup_draw(&d->setup,d->palette,&f);n=rc_tui_text(&d->setup.tui,out,cap);}
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
static int restart(desktop *d,char *error,size_t cap){
    rc_machine *next=rc_machine_create(d->config,d->roms,error,cap);if(!next)return 0;
    if(strcmp(d->config->model,"test")&&strcmp(rc_machine_family(next)->id,"c64")){rc_machine_destroy(next);snprintf(error,cap,"Required firmware is missing");return 0;}
    release_keys(d);rc_machine_destroy(d->machine);*d->owner=d->machine=next;rc_keymap_init(&d->keymap,next,d->config);rc_machine_border(next,d->config->border);d->running_config=*d->config;d->pace_dirty=1;return 1;
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
    if(!strcmp(bare,"F12")){
        if(!down)return 1;
        if(alt||ctrl){reset(d,ctrl?1:0);message(d,ctrl?"Cold boot":"Reset");return 1;}
        if(shifted){d->quit=1;return 2;}
        if(!d->setup.active){release_keys(d);d->setup.active=1;d->setup.row=0;}
        else {d->setup.active=0;if(d->restart_pending){if(!restart(d,error,sizeof error)){restore_machine_config(d);message(d,error);}d->restart_pending=0;}if(!store_config(d,error,sizeof error))message(d,error);}
        d->pace_dirty=1;input_config(d);return 1;
    }
    if(d->setup.active){
        if(!down)return 1;const rc_config before=*d->config;const unsigned action=rc_setup_key(&d->setup,bare);
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
    if(ctrl&&(!strcmp(bare,"F1")||!strcmp(bare,"F2")||!strcmp(bare,"F5")||!strcmp(bare,"F6"))){if(down)message(d,bare[1]=='1'||bare[1]=='2'?"No media device available":"Snapshot slots unavailable");return 1;}
    if((alt&&(!strcmp(bare,"F")||!strcmp(bare,"S")||!strcmp(bare,"T")))||!strcmp(bare,"F9")){if(down)message(d,"Control unavailable");return 1;}
    if(!strcmp(bare,"PRINTSCREEN")||!strcmp(bare,"PRINT SCREEN")){
        if(down){char path[RC_HOST_PATH];rc_host_make_folders("work/screenshots");snprintf(path,sizeof path,"work/screenshots/rec64-%llu.png",(unsigned long long)d->frames);if(shot(d,path,error,sizeof error))message(d,path);else message(d,error);}return 1;
    }
    /* Headless host-key commands use the same configurable physical map. */
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
    rc_machine *m=*owner;desktop d={0};d.machine=m;d.owner=owner;d.config=c;d.roms=roms;d.hidden=o->hidden;d.running_config=*c;
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
        host.key=shell_key;host.screen_text=screen_text;
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
                if(change==RC_PADS_KEY&&(d.setup.active||pad==RC_PAD_START)){
                    const char *names[]={"","UP","DOWN","LEFT","RIGHT","RETURN","ESCAPE","-","+","F12",""};if(pad!=RC_PAD_NONE&&pad!=RC_PAD_BACK)shell_key(&d,names[pad],1);}
                if(e.type==SDL_WINDOWEVENT&&e.window.event==SDL_WINDOWEVENT_FOCUS_LOST)release_keys(&d);
                else if(e.type==SDL_TEXTINPUT&&!d.setup.active&&!d.paused){if(!d.suppress_text&&!rc_keymap_text(&d.keymap,e.text.text))message(&d,"Character unavailable on C64 keyboard");}
                else if(e.type==SDL_MOUSEMOTION||e.type==SDL_MOUSEBUTTONDOWN||e.type==SDL_MOUSEBUTTONUP){
                    if(!d.setup.active&&!d.paused){const unsigned held=SDL_GetMouseState(NULL,NULL),buttons=(held&SDL_BUTTON_LMASK?1u:0)|(held&SDL_BUTTON_RMASK?2u:0);
                        for(unsigned p=0;p<2;++p)if(c->sticks[p].type==RC_CONTROL_MOUSE1351)rc_machine_pointer(d.machine,p,RC_CONTROL_MOUSE1351,e.type==SDL_MOUSEMOTION?e.motion.xrel:0,e.type==SDL_MOUSEMOTION?e.motion.yrel:0,buttons);}}
                else if(e.type==SDL_KEYDOWN||e.type==SDL_KEYUP){
                    if(e.key.repeat&&(!d.setup.active||(e.key.keysym.mod&(KMOD_ALT|KMOD_CTRL))||e.key.keysym.scancode==SDL_SCANCODE_F12))continue;
                    const int down=e.type==SDL_KEYDOWN;const char *name=SDL_GetScancodeName(e.key.keysym.scancode);
                    char hostkey[96];snprintf(hostkey,sizeof hostkey,"%s%s%s%s",e.key.keysym.mod&KMOD_CTRL?"Ctrl+":"",e.key.keysym.mod&KMOD_ALT?"Alt+":"",e.key.keysym.mod&KMOD_SHIFT?"Shift+":"",name);
                    /* Always release the original binding, even if modifiers changed. */
                    if(!down){rc_joykeys_key(&d.joykeys,(int)e.key.keysym.scancode,0);rc_keymap_key(&d.keymap,(unsigned)e.key.keysym.scancode,name,0);}
                    const int handled=shell_key(&d,hostkey,down);
                    if(down)d.suppress_text=handled!=0;
                    if(handled||d.paused)continue;
                    if(rc_joykeys_key(&d.joykeys,(int)e.key.keysym.scancode,down)){d.suppress_text=down;continue;}
                    const int mapped=rc_keymap_key(&d.keymap,(unsigned)e.key.keysym.scancode,name,down);
                    if(down)d.suppress_text=mapped;
                }

            }
        }
        if(!o->hidden){rc_pad_key pad;if(d.setup.active&&rc_pads_repeat(&d.pads,SDL_GetTicks(),&pad)){const char *names[]={"","UP","DOWN","LEFT","RIGHT"};shell_key(&d,names[pad],1);}input_poll(&d);}
        m=d.machine;
        if(d.pace_dirty){rc_pace_init(&pace,SDL_GetPerformanceFrequency(),rc_machine_clock(m),100);rc_pace_reset(&pace,SDL_GetPerformanceCounter(),rc_machine_time(m));d.pace_dirty=0;}
        uint64_t ticks=rc_machine_frame_period(m);int present=1;
        if(h) {
            const int action=rc_harness_step(h);
            if(action==RC_HARNESS_QUIT) { status=rc_harness_status(h);if(status) snprintf(error,cap,"%s",rc_harness_error(h));break; }
            if(action==RC_HARNESS_IDLE) { rc_host_sleep_us(1000);continue; }
        } else if(!o->hidden&&(d.setup.active||d.paused)) {rc_host_sleep_us(16000);d.pace_dirty=1;}
        else if(!o->hidden) {
            const rc_pace_step step=rc_pace_next(&pace,d.full,SDL_GetPerformanceCounter(),rc_machine_time(m),ticks);
            if(step.action==RC_PACE_WAIT) { rc_host_sleep_us(500);continue; }
            present=step.action==RC_PACE_PRESENT;ticks=present?0:step.ticks;
        }
        if(d.quit){status=0;break;}m=d.machine;
        if(!d.setup.active&&!d.paused&&rc_machine_run(m,ticks)!=RC_RUN_OK) { snprintf(error,cap,"%s",rc_machine_error(m));goto done; }
        if(present){++d.frames;if(!d.setup.active&&!d.paused)rc_keymap_frame(&d.keymap);}
        if(d.wav.file) rc_wav_pump(&d.wav,m);
        else rc_host_audio_pump(&d.audio,m,(unsigned)c->volume,o->hidden||d.full||!c->sound||d.setup.active||d.paused);
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
    if(!rc_wav_close(&d.wav)) { snprintf(error,cap,"Cannot finish audio capture");status=1; }
    rc_harness_close(h);rc_host_stream_close(d.tcp);rc_host_audio_close(&d.audio);
    if(!o->hidden){SDL_StopTextInput();rc_pads_close(&d.pads);}rc_video_close(d.video);free(d.pixels);SDL_Quit();return status;
}
