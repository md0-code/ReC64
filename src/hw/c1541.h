/* The 1541 board: one 1 MHz CPU, two VIAs and a rotating GCR mechanism.
 * Mounted rc_media objects are borrowed; close/eject never frees them.
 * Call flush before writing the borrowed image to its host file. */
#ifndef RC_C1541_H
#define RC_C1541_H
#include "mos6510.h"
#include "via6522.h"
#include "iec.h"
#include "../core/media/d64.h"
#include "../core/media/g64.h"
#include "../core/media/gcr.h"
typedef struct rc_c1541_status {unsigned device,halftrack,motor,led,writing;uint64_t steps,bumps,cycles;} rc_c1541_status;
typedef struct rc_c1541 {
    rc_mos6510 cpu;rc_via6522 via[2];
    uint8_t ram[2048],rom[16384];
    rc_media *image;rc_media *owned_image;rc_d64 disk;rc_g64 g64;
    /* Physical half-track indices 2..85: 1.0..42.5. */
    rc_gcr_track track[86];
    uint64_t cycles,steps,bumps;
    uint32_t bit_ticks,separator_ticks;uint8_t bit_count,read_shift,write_shift,read_latch,ones,separator_bit;
    uint8_t halftrack,phase,motor,led,writing,byte_ready,iec_in,iec_out,device,enabled,rotation_track;
    uint8_t dirty_track[86];
} rc_c1541;
void rc_c1541_init(rc_c1541 *d,const uint8_t rom[16384],unsigned device);
void rc_c1541_close(rc_c1541 *d);
void rc_c1541_reset(rc_c1541 *d);
void rc_c1541_tick(rc_c1541 *d);
void rc_c1541_iec_input(rc_c1541 *d,unsigned physical_lines);
unsigned rc_c1541_iec_output(const rc_c1541 *d);
void rc_c1541_device(rc_c1541 *d,unsigned device);
int rc_c1541_mount(rc_c1541 *d,rc_media *image,char *error,size_t cap);
rc_media *rc_c1541_image(const rc_c1541 *d);
int rc_c1541_flush(rc_c1541 *d);
rc_c1541_status rc_c1541_get_status(const rc_c1541 *d);
uint8_t rc_c1541_read(rc_c1541 *d,uint16_t address);
void rc_c1541_write(rc_c1541 *d,uint16_t address,uint8_t value);
void rc_c1541_save(const rc_c1541 *d,rc_state_writer *w);
int rc_c1541_load(rc_c1541 *d,rc_state_reader *r);
#endif
