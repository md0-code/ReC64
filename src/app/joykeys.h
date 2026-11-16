/* The keyboard joystick: six keys of the host's keyboard (up, down, left,
 * right, fire, fire 2; controllers.keys in the configuration) that, while it
 * is switched on, are a joystick's switches and no longer reach the
 * machine's keyboard. The shell's F11 key switches it on and off at any
 * time. No SDL: keys are the host's scancodes as integers, resolved from the
 * configuration's key names by the shell.  */
#ifndef RC_JOYKEYS_H
#define RC_JOYKEYS_H

#include "../core/config.h"

typedef struct rc_joykeys {
    int key[RC_CONFIG_STICK_KEYS];      /* the host key of each function (RC_STICK_UP...), -1: none */
    uint8_t down[RC_CONFIG_STICK_KEYS]; /* pressed while the joystick was on and not let go yet */
    int on;
} rc_joykeys;

/* `keys` are the host scancodes in the order of RC_STICK_UP to RC_STICK_FIRE2. */
void rc_joykeys_init(rc_joykeys *j, const int keys[RC_CONFIG_STICK_KEYS], int on);
/* New keys (a changed configuration): nothing is held afterwards. */
void rc_joykeys_set_keys(rc_joykeys *j, const int keys[RC_CONFIG_STICK_KEYS]);

/* A key of the host went down or up (not its repeats). 1 when the joystick took it, so it does
 * not reach the machine: a press while the joystick is on, and the release of a key it took. A key
 * pressed before the joystick was switched on is the machine's until it is let go. */
int rc_joykeys_key(rc_joykeys *j, int scancode, int down);

/* Switches it on or off; the directions and buttons held are let go either way. Returns 1 when on. */
int rc_joykeys_switch(rc_joykeys *j);
void rc_joykeys_set(rc_joykeys *j, int on);
int rc_joykeys_on(const rc_joykeys *j);

/* The directions and buttons held now, RC_JOY_* (machine.h); opposite directions cancel. */
unsigned rc_joykeys_held(const rc_joykeys *j);

#endif /* RC_JOYKEYS_H */
