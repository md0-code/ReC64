/* The host's game controllers (pads.h). Host controller interface. */
#include "pads.h"
#include "../core/machine.h"
#include "../core/stick.h"

#include <SDL.h>

#include <stdlib.h>
#include <string.h>

#define REPEAT_FIRST_MS 400
#define REPEAT_NEXT_MS 120

static SDL_GameController *controller(const rc_pads *p, unsigned index) {
    return index < RC_PADS_MAX ? (SDL_GameController *)p->pad[index] : NULL;
}

static int slot_for(const rc_pads *p, SDL_JoystickID id) {
    for (unsigned i = 0; i < RC_PADS_MAX; ++i)
        if (controller(p, i) && SDL_JoystickInstanceID(SDL_GameControllerGetJoystick(controller(p, i))) == id) return (int)i;
    return -1;
}

/* The host's device `device_index` into the first free number; 1 when a controller was added. */
static int add(rc_pads *p, int device_index) {
    if (!SDL_IsGameController(device_index)) return 0;
    unsigned free_number = 0;
    while (free_number < RC_PADS_MAX && p->pad[free_number]) ++free_number;
    if (free_number == RC_PADS_MAX) return 0;
    SDL_GameController *c = SDL_GameControllerOpen(device_index);
    if (!c) return 0;
    if (slot_for(p, SDL_JoystickInstanceID(SDL_GameControllerGetJoystick(c))) >= 0) { SDL_GameControllerClose(c); return 0; }
    p->pad[free_number] = c;
    return 1;
}

int rc_pads_open(rc_pads *p) {
    memset(p, 0, sizeof *p);
    p->held = RC_PAD_NONE;
    if (SDL_InitSubSystem(SDL_INIT_GAMECONTROLLER)) return 0;
    for (int i = 0; i < SDL_NumJoysticks(); ++i) add(p, i);
    return 1;
}

void rc_pads_close(rc_pads *p) {
    for (unsigned i = 0; i < RC_PADS_MAX; ++i) {
        if (controller(p, i)) SDL_GameControllerClose(controller(p, i));
        p->pad[i] = NULL;
    }
    SDL_QuitSubSystem(SDL_INIT_GAMECONTROLLER);
}

rc_pads_result rc_pads_event(rc_pads *p, const union SDL_Event *e, rc_pad_key *key) {
    rc_pad_key ignored;
    if (!key) key = &ignored;
    *key = RC_PAD_NONE;
    if (e->type == SDL_CONTROLLERDEVICEADDED) return add(p, e->cdevice.which) ? RC_PADS_CHANGED : RC_PADS_NONE;
    if (e->type == SDL_CONTROLLERDEVICEREMOVED) {
        for (unsigned i = 0; i < RC_PADS_MAX; ++i) {
            if (!controller(p, i) || SDL_JoystickInstanceID(SDL_GameControllerGetJoystick(controller(p, i))) != e->cdevice.which) continue;
            SDL_GameControllerClose(controller(p, i));
            p->pad[i] = NULL;
            memset(p->trigger[i],0,sizeof p->trigger[i]);
            p->start[i]=0;
            p->held = RC_PAD_NONE;
            return RC_PADS_CHANGED;
        }
        return RC_PADS_NONE;
    }
    if(e->type==SDL_CONTROLLERAXISMOTION&&(e->caxis.axis==SDL_CONTROLLER_AXIS_TRIGGERLEFT||e->caxis.axis==SDL_CONTROLLER_AXIS_TRIGGERRIGHT)){
        for(unsigned i=0;i<RC_PADS_MAX;++i)if(controller(p,i)&&SDL_JoystickInstanceID(SDL_GameControllerGetJoystick(controller(p,i)))==e->caxis.which){
            const unsigned trigger=e->caxis.axis==SDL_CONTROLLER_AXIS_TRIGGERLEFT?0u:1u;const int pressed=e->caxis.value>=RC_STICK_DEAD_ZONE;
            const int edge=pressed&&!p->trigger[i][trigger];p->trigger[i][trigger]=(uint8_t)pressed;
            if(edge){*key=trigger?RC_PAD_TRIGGER_RIGHT:RC_PAD_TRIGGER_LEFT;return RC_PADS_KEY;}break;
        }return RC_PADS_NONE;
    }
    if(e->type!=SDL_CONTROLLERBUTTONDOWN&&e->type!=SDL_CONTROLLERBUTTONUP)return RC_PADS_NONE;
    const int slot=slot_for(p,e->cbutton.which);
    if(slot<0)return RC_PADS_NONE;
    if(e->cbutton.button==SDL_CONTROLLER_BUTTON_START){
        if(e->type==SDL_CONTROLLERBUTTONDOWN){if(!p->start[slot])p->start[slot]=1;return RC_PADS_NONE;}
        const int tap=p->start[slot]==1;
        p->start[slot]=0;
        if(!tap)return RC_PADS_NONE;
        *key=RC_PAD_START;
        return RC_PADS_KEY;
    }
    if(e->type==SDL_CONTROLLERBUTTONUP)return RC_PADS_NONE;
    if(p->start[slot]&&(e->cbutton.button==SDL_CONTROLLER_BUTTON_X||
        e->cbutton.button==SDL_CONTROLLER_BUTTON_Y||e->cbutton.button==SDL_CONTROLLER_BUTTON_RIGHTSTICK)){
        if(p->start[slot]==2)return RC_PADS_NONE;
        p->start[slot]=2;
        *key=e->cbutton.button==SDL_CONTROLLER_BUTTON_X?RC_PAD_EXIT:
            e->cbutton.button==SDL_CONTROLLER_BUTTON_Y?RC_PAD_COLD_BOOT:RC_PAD_SCREENSHOT;
        return RC_PADS_KEY;
    }
    switch (e->cbutton.button) {
    case SDL_CONTROLLER_BUTTON_A: *key = RC_PAD_A; break;
    case SDL_CONTROLLER_BUTTON_B: *key = RC_PAD_B; break;
    case SDL_CONTROLLER_BUTTON_X: *key = RC_PAD_X; break;
    case SDL_CONTROLLER_BUTTON_Y: *key = RC_PAD_Y; break;
    case SDL_CONTROLLER_BUTTON_LEFTSHOULDER: *key = RC_PAD_SHOULDER_LEFT; break;
    case SDL_CONTROLLER_BUTTON_RIGHTSHOULDER: *key = RC_PAD_SHOULDER_RIGHT; break;
    case SDL_CONTROLLER_BUTTON_GUIDE: *key = RC_PAD_START; break;
    case SDL_CONTROLLER_BUTTON_BACK: *key = RC_PAD_BACK; break;
    case SDL_CONTROLLER_BUTTON_LEFTSTICK: *key=RC_PAD_L3; break;
    case SDL_CONTROLLER_BUTTON_RIGHTSTICK: *key=RC_PAD_R3; break;
    default: return RC_PADS_NONE;       /* the d-pad repeats through rc_pads_repeat */
    }
    return RC_PADS_KEY;
}

/* The direction a d-pad or stick of any controller holds, RC_PAD_NONE for none. */
static int direction(const rc_pads *p) {
    for (unsigned i = 0; i < RC_PADS_MAX; ++i) {
        SDL_GameController *c = controller(p, i);
        if (!c) continue;
        if (SDL_GameControllerGetButton(c, SDL_CONTROLLER_BUTTON_DPAD_UP)) return RC_PAD_UP;
        if (SDL_GameControllerGetButton(c, SDL_CONTROLLER_BUTTON_DPAD_DOWN)) return RC_PAD_DOWN;
        if (SDL_GameControllerGetButton(c, SDL_CONTROLLER_BUTTON_DPAD_LEFT)) return RC_PAD_LEFT;
        if (SDL_GameControllerGetButton(c, SDL_CONTROLLER_BUTTON_DPAD_RIGHT)) return RC_PAD_RIGHT;
        const int x = SDL_GameControllerGetAxis(c, SDL_CONTROLLER_AXIS_LEFTX), y = SDL_GameControllerGetAxis(c, SDL_CONTROLLER_AXIS_LEFTY);
        if (abs(x) >= RC_STICK_DEAD_ZONE || abs(y) >= RC_STICK_DEAD_ZONE) {
            if (abs(y) >= abs(x)) return y < 0 ? RC_PAD_UP : RC_PAD_DOWN;
            return x < 0 ? RC_PAD_LEFT : RC_PAD_RIGHT;
        }
    }
    return RC_PAD_NONE;
}

int rc_pads_repeat(rc_pads *p, uint32_t now_ms, rc_pad_key *key) {
    const int d = direction(p);
    if (d == RC_PAD_NONE) { p->held = RC_PAD_NONE; return 0; }
    if (d != p->held) {
        p->held = d;
        p->next_ms = now_ms + REPEAT_FIRST_MS;
    } else if ((int32_t)(now_ms - p->next_ms) >= 0) p->next_ms = now_ms + REPEAT_NEXT_MS;
    else return 0;
    *key = (rc_pad_key)d;
    return 1;
}

int rc_pads_present(const rc_pads *p, unsigned index) { return controller(p, index) != NULL; }
int rc_pads_any(const rc_pads *p){for(unsigned i=0;i<RC_PADS_MAX;++i)if(rc_pads_present(p,i))return 1;return 0;}
int rc_pads_keyboard(const rc_pads *p,int requested){return requested&&!rc_pads_any(p);}

const char *rc_pads_name(const rc_pads *p, unsigned index) {
    SDL_GameController *c = controller(p, index);
    const char *name = c ? SDL_GameControllerName(c) : NULL;
    if (!c) return "";
    return name && *name ? name : "Game controller";
}

unsigned rc_pads_joystick(const rc_pads *p, unsigned index) {
    SDL_GameController *c = controller(p, index);
    if (!c) return 0;
    unsigned held = 0;
    if (SDL_GameControllerGetButton(c, SDL_CONTROLLER_BUTTON_DPAD_UP)) held |= RC_JOY_UP;
    if (SDL_GameControllerGetButton(c, SDL_CONTROLLER_BUTTON_DPAD_DOWN)) held |= RC_JOY_DOWN;
    if (SDL_GameControllerGetButton(c, SDL_CONTROLLER_BUTTON_DPAD_LEFT)) held |= RC_JOY_LEFT;
    if (SDL_GameControllerGetButton(c, SDL_CONTROLLER_BUTTON_DPAD_RIGHT)) held |= RC_JOY_RIGHT;
    held |= rc_stick_directions(SDL_GameControllerGetAxis(c, SDL_CONTROLLER_AXIS_LEFTX), SDL_GameControllerGetAxis(c, SDL_CONTROLLER_AXIS_LEFTY));
    if (SDL_GameControllerGetButton(c, SDL_CONTROLLER_BUTTON_A)) held |= RC_JOY_FIRE;
    return rc_stick_resolve(held);
}

int rc_host_key_known(const char *name) { return SDL_GetScancodeFromName(name) != SDL_SCANCODE_UNKNOWN; }

void rc_pads_paddles(const rc_pads *p,unsigned index,int *x,int *y,unsigned *buttons){
    SDL_GameController *c=controller(p,index);*x=*y=128;*buttons=0;if(!c)return;
    *x=(SDL_GameControllerGetAxis(c,SDL_CONTROLLER_AXIS_LEFTX)+32768)/256;
    *y=(SDL_GameControllerGetAxis(c,SDL_CONTROLLER_AXIS_LEFTY)+32768)/256;
    *buttons=SDL_GameControllerGetButton(c,SDL_CONTROLLER_BUTTON_A)?1u:0;
}
