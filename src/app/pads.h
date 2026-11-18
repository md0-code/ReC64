/* The host's game controllers (SDL's GameController API) as joysticks of
 * the machine: a controller's d-pad and left stick (beyond a dead zone,
 * stick.h) are the directions and A is fire. They also work without a video subsystem. A controller keeps its
 * number (1 to RC_PADS_MAX, in the order the host reported them) while it
 * is plugged in; one that is unplugged leaves its number empty, and its
 * socket released, until the next controller takes the first free one.
 *
 * rc_pads_event and rc_pads_repeat also turn controller input into the
 * events a setup screen takes: d-pad and stick move, A accepts, and
 * the remaining buttons invoke the shell's menu, media and state controls.
 *
 * pads.c is the only file besides the desktop that uses SDL; this header
 * needs none. Host controller interface. */
#ifndef RC_PADS_H
#define RC_PADS_H

#include "../core/config.h"

#include <stdint.h>

#define RC_PADS_MAX RC_CONFIG_PADS

/* What a setup screen takes from a controller. */
typedef enum rc_pad_key {
    RC_PAD_NONE,
    RC_PAD_UP, RC_PAD_DOWN, RC_PAD_LEFT, RC_PAD_RIGHT,
    RC_PAD_A,                           /* accept */
    RC_PAD_B,                           /* F12 menu or cancel a modal picker */
    RC_PAD_SHOULDER_LEFT,               /* switch control port */
    RC_PAD_SHOULDER_RIGHT,              /* full speed */
    RC_PAD_START,                       /* open and close the setup screen (the Guide button too) */
    RC_PAD_BACK,                        /* Select / Back: on-screen keyboard */
    RC_PAD_X, RC_PAD_Y, RC_PAD_TRIGGER_LEFT, RC_PAD_TRIGGER_RIGHT,
    RC_PAD_COLD_BOOT, RC_PAD_EXIT, RC_PAD_L3, RC_PAD_R3, RC_PAD_SCREENSHOT
} rc_pad_key;

typedef struct rc_pads {
    void *pad[RC_PADS_MAX];             /* SDL_GameController *, NULL for a number with no controller */
    int held;                           /* the direction held (a rc_pad_key), RC_PAD_NONE for none */
    uint32_t next_ms;                   /* when a held direction repeats */
    uint8_t trigger[RC_PADS_MAX][2];    /* rising edges, one action per squeeze */
    uint8_t start[RC_PADS_MAX];         /* idle, held for a tap, held after a shortcut */
} rc_pads;

typedef enum rc_pads_result {
    RC_PADS_NONE,
    RC_PADS_CHANGED,                    /* a controller came or went */
    RC_PADS_KEY                         /* a setup screen key is set */
} rc_pads_result;

/* Starts the controller subsystem and opens those present; 0 when SDL has none to offer. */
int rc_pads_open(rc_pads *p);
void rc_pads_close(rc_pads *p);

/* A controller event (device added or removed, button down); anything else is ignored. `key` may be NULL. */
union SDL_Event;
rc_pads_result rc_pads_event(rc_pads *p, const union SDL_Event *e, rc_pad_key *key);
/* A direction held on a d-pad or stick repeats like a key: after 400 ms, then every 120. 1 when `key` is set. */
int rc_pads_repeat(rc_pads *p, uint32_t now_ms, rc_pad_key *key);

/* 1 when controller `index` (0 to RC_PADS_MAX - 1: the number minus one) is plugged in; its name ("" when it is not). */
int rc_pads_present(const rc_pads *p, unsigned index);
int rc_pads_any(const rc_pads *p);
int rc_pads_keyboard(const rc_pads *p,int requested);
const char *rc_pads_name(const rc_pads *p, unsigned index);
/* What controller `index` holds as a joystick: RC_JOY_* bits (machine.h), 0 for none or an empty number. */
unsigned rc_pads_joystick(const rc_pads *p, unsigned index);

/* Whether the host has a key of this name (SDL's names: "Up", "Left Ctrl"); what the configuration's
 * reader checks controllers.keys with (rc_config_set_key_check). Needs no rc_pads_open. */
/* Signed left-stick axes and A button, for analogue paddles. */
void rc_pads_paddles(const rc_pads *p,unsigned index,int *x,int *y,unsigned *buttons);
int rc_host_key_known(const char *name);

#endif /* RC_PADS_H */
