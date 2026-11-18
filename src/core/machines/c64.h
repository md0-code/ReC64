#ifndef RC_C64_H
#define RC_C64_H
#include "../machine_impl.h"
#include "../../hw/bus.h"
#include "../../hw/sched.h"
#include "../../hw/traps.h"
#include "../../hw/datasette.h"
#include "../../hw/c1541.h"
#include "../../hw/fastload.h"
#include "../../hw/drivesnd.h"
#include "../../hw/cart.h"
#include "../../hw/cart_freezer.h"
#include "../../hw/easyflash.h"
#include "../../hw/gmod2.h"
#include "../../hw/reu.h"
#include "../../hw/georam.h"
#include "c64_sid.h"
typedef struct rc_c64 {
    rc_hw_host host;rc_sched sched;rc_bus bus;
    rc_mos6510 cpu;rc_vic2 vic;rc_cia6526 cia[2];rc_c64kbd keyboard;rc_pla pla;rc_sid sid;
    uint8_t model,restore,port_charge,border;uint32_t hz;uint64_t tod_at;
    uint8_t rom_hashes[128],frame[520u*312u];
    /* The last raster the VIC completed: the picture never shows one in progress. */
    uint8_t shown[RC_VIC_MAX_PIXELS];uint64_t shown_frames;
    rc_c1541 drive;rc_iec iec;rc_fastload fastload;rc_drivesnd drive_sound;uint64_t drive_phase;
    uint8_t disk_audio_engaged; /* IEC attention has requested the mounted drive since reset. */
    rc_datasette tape;rc_traps traps;rc_cpu_hooks hooks;uint8_t fetch_watch[8192];
    rc_cart cart;rc_freezer freezer;rc_easyflash easyflash;rc_gmod2 gmod2;rc_media *cart_eeprom;rc_reu reu;rc_georam georam;uint32_t freeze_cycles;
    rc_sid_session sid_session;
    rc_media *media[5],*synthetic;uint8_t media_owned[5],source,physical;
    uint32_t selected[5],tape_bits[5],program_size;uint16_t program_address;
    uint8_t *program;uint8_t program_name[16],disk_name[16];uint16_t disk_address;char autoload_text[512];
    uint64_t tape_audio_phase;int16_t tape_audio[48000];unsigned tape_audio_head,tape_audio_count;
    rc_trace_fn trace;void *trace_ctx;
} rc_c64;
extern const rc_family rc_c64_family;
void *rc_c64_create(const rc_machine_setup *setup,char *error,size_t capacity);
rc_run_status rc_c64_run(void *self,uint64_t cycles);
#endif
