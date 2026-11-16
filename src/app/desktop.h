#ifndef RC_DESKTOP_H
#define RC_DESKTOP_H
#include "../core/machine.h"
typedef struct rc_desktop_options {
    int hidden;
    uint64_t frames;
    const char *script, *control, *screenshot, *audio;
} rc_desktop_options;
int rc_desktop_run(rc_machine *machine, const rc_config *config, const rc_desktop_options *options, char *error, size_t capacity);
#endif
