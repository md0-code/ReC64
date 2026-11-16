/* The host loop. Headless runs never initialise SDL video or create a window. */
#include "desktop.h"
#include "harness.h"
#include "pace.h"
#include "../host/host.h"
#include "../platform/video.h"
#include "../platform/png.h"
#include "../platform/audio.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
typedef struct desktop {
    rc_machine *machine;
    rc_video *video;
    rc_host_stream *tcp;
    rc_host_audio audio;
    rc_wav wav;
    uint32_t *pixels;
    size_t pixel_count;
    char input[1024];
    size_t input_size;
    int stdin_control, full, io_failed;
} desktop;
static rc_machine *machine(void *ctx) { return ((desktop *)ctx)->machine; }
static int picture(desktop *d,rc_frame *f,char *error,size_t cap) {
    if(!rc_machine_frame(d->machine,f)) { snprintf(error,cap,"No machine frame");return 0; }
    const size_t n=(size_t)f->width*f->height;
    if(n>d->pixel_count) {
        uint32_t *p=realloc(d->pixels,n*sizeof *p);
        if(!p) { snprintf(error,cap,"Out of memory");return 0; }
        d->pixels=p;d->pixel_count=n;
    }
    rc_frame_argb(f,d->pixels);return 1;
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
    free(data);return ok;
}
static void reset(void *ctx,int power) { rc_machine_reset(((desktop *)ctx)->machine,power==1); }
static void speed(void *ctx,int full) { ((desktop *)ctx)->full=full; }
static int audio_capture(void *ctx,const char *path,char *error,size_t cap) {
    desktop *d=ctx;
    if(!rc_wav_close(&d->wav)) { snprintf(error,cap,"Cannot finish audio capture");return 0; }
    if(!strcmp(path,"off")) return 1;
    if(!rc_wav_open(&d->wav,path)) { snprintf(error,cap,"Cannot open audio capture");return 0; }
    return 1;
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
int rc_desktop_run(rc_machine *m,const rc_config *c,const rc_desktop_options *o,char *error,size_t cap) {
    desktop d={0};d.machine=m;int status=1;rc_harness *h=NULL;uint64_t frames=0;
    const int driven=o->script||o->control;
    if(SDL_Init(o->hidden?SDL_INIT_TIMER:SDL_INIT_VIDEO|SDL_INIT_AUDIO|SDL_INIT_TIMER)!=0) {
        snprintf(error,cap,"SDL: %s",SDL_GetError());return 1;
    }
    if(!o->hidden) {
        const rc_settings settings={c->fullscreen!=0,2,c->scale==RC_CONFIG_SCALE_INTEGER?8:0,(int)c->aspect,
            (int)c->filter,c->look==RC_LOOK_CRT,c->vsync!=0};
        rc_frame f;if(!picture(&d,&f,error,cap)) goto done;
        d.video=rc_video_open("ReC64",&settings,(int)f.width,(int)f.height,(int)f.aspect_w,(int)f.aspect_h,false,error,cap);
        if(!d.video) goto done;
        if(c->sound) rc_host_audio_open(&d.audio);
    }
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
        if(o->control) { host.control_read=control_read;host.control_write=control_write; }
        h=rc_harness_open(&host,o->script,error,cap);if(!h) goto done;
    }
    rc_pace pace;rc_pace_init(&pace,SDL_GetPerformanceFrequency(),rc_machine_clock(m),100);
    rc_pace_reset(&pace,SDL_GetPerformanceCounter(),rc_machine_time(m));
    for(;;) {
        if(o->frames&&frames>=o->frames) { status=0;break; }
        if(!o->hidden) {
            SDL_Event e;
            while(SDL_PollEvent(&e)) {
                if(e.type==SDL_QUIT) { status=0;goto done; }
                if(e.type==SDL_KEYDOWN&&!e.key.repeat&&e.key.keysym.sym==SDLK_RETURN&&(e.key.keysym.mod&KMOD_ALT))
                    rc_video_set_fullscreen(d.video,!rc_video_fullscreen(d.video));
                else if(e.type==SDL_KEYDOWN||e.type==SDL_KEYUP) {
                    const rc_family *f=rc_machine_family(m);
                    for(unsigned k=0;k<f->key_count;++k)
                        if(f->keys[k].host&&SDL_GetScancodeFromName(f->keys[k].host)==e.key.keysym.scancode)
                            rc_machine_key(m,k,e.type==SDL_KEYDOWN);
                }
            }
        }
        uint64_t ticks=rc_machine_frame_period(m);int present=1;
        if(h) {
            const int action=rc_harness_step(h);
            if(action==RC_HARNESS_QUIT) { status=rc_harness_status(h);if(status) snprintf(error,cap,"%s",rc_harness_error(h));break; }
            if(action==RC_HARNESS_IDLE) { rc_host_sleep_us(1000);continue; }
        } else if(!o->hidden) {
            const rc_pace_step step=rc_pace_next(&pace,d.full,SDL_GetPerformanceCounter(),rc_machine_time(m),ticks);
            if(step.action==RC_PACE_WAIT) { rc_host_sleep_us(500);continue; }
            present=step.action==RC_PACE_PRESENT;ticks=present?0:step.ticks;
        }
        if(rc_machine_run(m,ticks)!=RC_RUN_OK) { snprintf(error,cap,"%s",rc_machine_error(m));goto done; }
        if(present) ++frames;
        if(d.wav.file) rc_wav_pump(&d.wav,m);
        else rc_host_audio_pump(&d.audio,m,(unsigned)c->volume,o->hidden||d.full||!c->sound);
        if(d.video&&present) {
            rc_frame f;if(!picture(&d,&f,error,cap)) goto done;
            rc_video_present(d.video,d.pixels,(int)f.width,(int)f.height);
        }
        if(d.io_failed) { snprintf(error,cap,"Control channel failed");goto done; }
    }
    if(!status&&o->screenshot&&!shot(&d,o->screenshot,error,cap)) status=1;
done:
    if(d.io_failed) { snprintf(error,cap,"Control channel failed");status=1; }
    if(!rc_wav_close(&d.wav)) { snprintf(error,cap,"Cannot finish audio capture");status=1; }
    rc_harness_close(h);rc_host_stream_close(d.tcp);rc_host_audio_close(&d.audio);
    rc_video_close(d.video);free(d.pixels);SDL_Quit();return status;
}
