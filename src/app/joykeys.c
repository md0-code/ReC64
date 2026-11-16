/* The keyboard joystick (joykeys.h). Configurable keyboard joystick. */
#include "joykeys.h"
#include "../core/machine.h"
#include "../core/stick.h"

#include <string.h>

void rc_joykeys_set_keys(rc_joykeys *j, const int keys[RC_CONFIG_STICK_KEYS]) {
    memcpy(j->key, keys, sizeof j->key);
    memset(j->down, 0, sizeof j->down);
}

void rc_joykeys_init(rc_joykeys *j, const int keys[RC_CONFIG_STICK_KEYS], int on) {
    memset(j, 0, sizeof *j);
    rc_joykeys_set_keys(j, keys);
    j->on = on != 0;
}

static int function_of(const rc_joykeys *j, int scancode) {
    if (scancode < 0) return -1;
    for (int i = 0; i < RC_CONFIG_STICK_KEYS; ++i) if (j->key[i] == scancode) return i;
    return -1;
}

int rc_joykeys_key(rc_joykeys *j, int scancode, int down) {
    const int f = function_of(j, scancode);
    if (f < 0) return 0;
    if (down) {
        if (!j->on) return 0;
        j->down[f] = 1;
        return 1;
    }
    if (!j->down[f]) return 0;
    j->down[f] = 0;
    return 1;
}

void rc_joykeys_set(rc_joykeys *j, int on) {
    j->on = on != 0;
    memset(j->down, 0, sizeof j->down);
}

int rc_joykeys_switch(rc_joykeys *j) {
    rc_joykeys_set(j, !j->on);
    return j->on;
}

int rc_joykeys_on(const rc_joykeys *j) { return j->on; }

unsigned rc_joykeys_held(const rc_joykeys *j) {
    static const unsigned BITS[RC_CONFIG_STICK_KEYS] = {RC_JOY_UP, RC_JOY_DOWN, RC_JOY_LEFT, RC_JOY_RIGHT, RC_JOY_FIRE, RC_JOY_FIRE2};
    unsigned held = 0;
    for (int i = 0; i < RC_CONFIG_STICK_KEYS; ++i) if (j->down[i]) held |= BITS[i];
    return rc_stick_resolve(held);
}
