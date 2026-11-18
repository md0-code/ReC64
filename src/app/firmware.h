/* The firmware ReC64 needs, and the screen that asks for it. ReC64 ships no
 * ROM: BASIC, KERNAL, the character set and at least one of the two drive
 * ROMs have to be supplied, and are recognised by content. */
#ifndef RC_FIRMWARE_H
#define RC_FIRMWARE_H
#include "tui.h"
/* 1 when the three system ROMs and a drive ROM were found. */
int rc_firmware_complete(const rc_rom_index *roms);
/* 1 when both drive ROMs were found, so that the drive type is a choice. */
int rc_firmware_drive_choice(const rc_rom_index *roms);
/* With a single drive ROM the configuration takes that drive. */
void rc_firmware_apply(rc_config *config,const rc_rom_index *roms);
/* What was found and what is missing, as lines for a console. */
void rc_firmware_report(const rc_rom_index *roms,const char *folder,char *out,size_t capacity);
/* The screen listing the ROMs and the folder they belong in. */
void rc_firmware_draw(rc_tui *t,const rc_rom_index *roms,const char *folder);
#endif
