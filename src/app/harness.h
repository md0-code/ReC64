/* Script and JSON-line control driver, independent of SDL and the OS.
 * Commands: wait, frames, type, enter, key, release, joy, insert, eject,
 * drop, play, tape, nmi, reset, shot, text, expect, until, state, speed,
 * poke, peek, trace, audio and quit. Unavailable hardware returns an error.
 * A script stops on failure; a control command returns {ok,frame,text|data}
 * or {ok:false,error}. Guest time stands still between hidden commands. */
#ifndef RC_HARNESS_H
#define RC_HARNESS_H
#include "../core/machine.h"
typedef struct rc_harness rc_harness;
typedef struct rc_harness_host {
    void *ctx;
    rc_machine *(*machine)(void *ctx);
    int (*insert)(void *ctx,unsigned slot,const char *path,char *error,size_t capacity);
    int (*eject)(void *ctx,unsigned slot);
    int (*shot)(void *ctx,const char *path,char *error,size_t capacity);
    int (*state_save)(void *ctx,const char *path,char *error,size_t capacity);
    int (*state_load)(void *ctx,const char *path,char *error,size_t capacity);
    void (*reset)(void *ctx,int power_on);
    int (*control_read)(void *ctx,char *line,size_t capacity);
    void (*control_write)(void *ctx,const char *line);
    int deterministic;
    void (*speed)(void *ctx,int full);
    size_t (*screen_text)(void *ctx,char *out,size_t capacity);
    int (*drop)(void *ctx,const char *path,char *error,size_t capacity);
    int (*audio)(void *ctx,const char *path,char *error,size_t capacity);
    int (*text_input)(void *ctx,const char *text); /* 1: host UI consumed typing */
    int (*key)(void *ctx,const char *name,int down); /* 1: consumed; 2: power off */
} rc_harness_host;
rc_harness *rc_harness_open(const rc_harness_host *host,const char *script,char *error,size_t capacity);
void rc_harness_close(rc_harness *h);
enum {RC_HARNESS_RUN,RC_HARNESS_IDLE,RC_HARNESS_QUIT};
int rc_harness_step(rc_harness *h);
int rc_harness_status(const rc_harness *h);
const char *rc_harness_error(const rc_harness *h);
#endif
