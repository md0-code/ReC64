#ifndef RC_DESKTOP_H
#define RC_DESKTOP_H
#include "../core/machine.h"
typedef struct rc_desktop_options {
    int hidden;
    uint64_t frames;
    const char *script, *control, *screenshot, *audio;
    /* The folders searched for ROMs, and the one a user is told to fill. */
    const char *const *rom_folders;
    size_t rom_folder_count;
    const char *rom_folder;
} rc_desktop_options;
int rc_desktop_run(rc_machine **machine,rc_config *config,rc_rom_index *roms,const rc_desktop_options *options,char *error,size_t capacity);
#endif
