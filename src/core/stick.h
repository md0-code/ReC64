/* A joystick's directions from the positions a host device reports. Every
 * joystick of these machines is digital (a switch for each direction and
 * for each fire button: RC_JOY_* in machine.h), so a game controller's
 * analogue stick becomes directions beyond a dead zone, and opposite
 * directions held together cancel as the switches of a real joystick
 * cannot close both. No SDL: the shell reads the host's axes and calls
 * these. 
 * dropped: the machines have no analogue joystick). */
#ifndef RC_STICK_H
#define RC_STICK_H

#include "machine.h"

/* A stick position runs from -32768 (left, up) to 32767 (right, down), 0 at rest. An axis counts
 * as a direction once it is this far (about half of the travel) from rest. */
#define RC_STICK_DEAD_ZONE 16000

/* The directions of a stick position: RC_JOY_LEFT or RC_JOY_RIGHT for x, RC_JOY_UP or RC_JOY_DOWN
 * for y, both axes at once for a diagonal. */
unsigned rc_stick_directions(int x, int y);

/* `held` (RC_JOY_* bits) with each pair of opposite directions both held removed. */
unsigned rc_stick_resolve(unsigned held);

#endif /* RC_STICK_H */
