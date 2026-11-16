/* Berkeley Softworks GeoRAM: I/O1 window, write-only I/O2 selectors. */
#ifndef RC_GEORAM_H
#define RC_GEORAM_H
#include "../core/state.h"
typedef struct rc_georam {uint8_t *ram;uint32_t size;uint8_t page,bank;} rc_georam;
/* Zero bytes disables it; otherwise 512 KiB, 1, 2 or 4 MiB. */
int rc_georam_init(rc_georam *g,uint32_t bytes);
void rc_georam_free(rc_georam *g);
/* Original 74HC378 latches and RAM have no reset input; retained on reset. */
void rc_georam_reset(rc_georam *g);
uint8_t rc_georam_drive_mask(const rc_georam *g,uint16_t address);
uint8_t rc_georam_read(const rc_georam *g,uint16_t address,uint8_t bus,int side_effects);
void rc_georam_write(rc_georam *g,uint16_t address,uint8_t value);
void rc_georam_save(const rc_georam *g,rc_state_writer *w);
int rc_georam_load(rc_georam *g,rc_state_reader *r);
#endif
