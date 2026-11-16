/* A joystick's directions from host positions (stick.h).  */
#include "stick.h"

unsigned rc_stick_directions(int x, int y) {
    unsigned held = 0;
    if (x <= -RC_STICK_DEAD_ZONE) held |= RC_JOY_LEFT;
    else if (x >= RC_STICK_DEAD_ZONE) held |= RC_JOY_RIGHT;
    if (y <= -RC_STICK_DEAD_ZONE) held |= RC_JOY_UP;
    else if (y >= RC_STICK_DEAD_ZONE) held |= RC_JOY_DOWN;
    return held;
}

unsigned rc_stick_resolve(unsigned held) {
    if ((held & (RC_JOY_LEFT | RC_JOY_RIGHT)) == (RC_JOY_LEFT | RC_JOY_RIGHT)) held &= ~(unsigned)(RC_JOY_LEFT | RC_JOY_RIGHT);
    if ((held & (RC_JOY_UP | RC_JOY_DOWN)) == (RC_JOY_UP | RC_JOY_DOWN)) held &= ~(unsigned)(RC_JOY_UP | RC_JOY_DOWN);
    return held;
}
