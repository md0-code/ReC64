/* Board-owned SID playback and deterministic session state. */
#ifndef RC_C64_SID_H
#define RC_C64_SID_H
#include "../media/sidfile.h"
#include "../media.h"
#include "../state.h"
struct rc_c64;
typedef struct rc_sid_session {
    rc_sidfile file;
    uint64_t elapsed_cycles,boot_cycles;
    uint16_t return_pc;
    uint8_t active,phase,song,paused,return_s,return_a,return_x,return_y,return_p;
} rc_sid_session;
int rc_c64_sid_insert(struct rc_c64 *c,rc_media *image,char *error,size_t cap);
void rc_c64_sid_eject(struct rc_c64 *c);
void rc_c64_sid_reset(struct rc_c64 *c);
void rc_c64_sid_tick(struct rc_c64 *c);
int rc_c64_sid_paused(const struct rc_c64 *c);
int rc_c64_sid_control(struct rc_c64 *c,unsigned command,uint32_t value);
void rc_c64_sid_save(const struct rc_c64 *c,rc_state_writer *w);
int rc_c64_sid_load(struct rc_c64 *c,rc_state_reader *r);
#endif
