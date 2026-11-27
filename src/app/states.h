/* Snapshot metadata and ten per-model slots. */
#ifndef RC_STATES_H
#define RC_STATES_H

#include "../core/config.h"
#include "../core/machine.h"
#include "../core/state.h"

#include <stddef.h>
#include <stdint.h>

#define RC_STATES_SLOTS 10
#define RC_STATES_THUMB_W 96            /* the largest thumbnail */
#define RC_STATES_THUMB_H 72
#define RC_STATES_VERSION 32

typedef struct rc_states_media {
    int present;
    char path[RC_MEDIA_PATH];
    int hashed;                         /* sha256 is the content's at the save (an older state has none) */
    uint8_t sha256[32];
    int write_protect;
} rc_states_media;

typedef struct rc_states_thumb {
    uint32_t width, height, colors;
    uint32_t palette[256];
    uint8_t pixels[RC_STATES_THUMB_W * RC_STATES_THUMB_H];
} rc_states_thumb;

typedef struct rc_states_time {
    uint16_t year;
    uint8_t month, day, hour, minute, second;
} rc_states_time;

/* What a state file says besides the machine's own chunks. */
typedef struct rc_states_info {
    char family[RC_STATE_ID], model[RC_STATE_ID];
    char version[RC_STATES_VERSION];    /* "" when the state does not say */
    int has_time;
    rc_states_time time;
    int has_thumb;
    rc_states_thumb thumb;
    rc_states_media media[RC_CONFIG_MEDIA];
} rc_states_info;

/* Final save for a session that is about to be destroyed. On a write failure,
 * recover every available dirty image under a fresh name beside base. */
int rc_states_finish_media(rc_machine *m, unsigned slot, rc_media *owned, const char *base, char *error, size_t capacity);

/* The thumbnail of a frame: a quarter of its size (at most RC_STATES_THUMB_W x RC_STATES_THUMB_H, its shape kept),
 * each pixel the frame's colour nearest to the average of the pixels it stands for. 0 when the frame has no pixels. */
int rc_states_thumb_make(const rc_frame *frame, rc_states_thumb *thumb);

/* Write the machine and optional metadata. Append shell chunks, then finish. */
int rc_states_write(const rc_machine *m, const rc_states_info *extra, rc_state_writer *w);
/* Opens a state for rc_machine_load (r) and reads the shell's chunks into info. 0 with the reason when it is not a state
 * ReC64 reads. `data` must outlive r. */
int rc_states_read(const uint8_t *data, size_t size, rc_states_info *info, rc_state_reader *r, char *error, size_t capacity);

/* --- files and slots ------------------------------------------------------------------- */

/* states/<model>/slot-<slot>.rcs beside the configuration file `config_source` (slot 1 to RC_STATES_SLOTS). */
void rc_states_slot_path(const char *config_source, const char *model, unsigned slot, char *out, size_t capacity);
int rc_states_finish(rc_state_writer *w);
/* A state file written whole, its folders made first. 0 with the reason. */
int rc_states_save_file(const char *path, const uint8_t *data, size_t size, char *error, size_t capacity);
/* A state file read whole (the caller frees *data). 0 with the reason. */
int rc_states_load_file(const char *path, uint8_t **data, size_t *size, char *error, size_t capacity);
/* What a slot file says (info), 0 when the slot is empty or its file is not a state. */
int rc_states_slot_info(const char *path, rc_states_info *info);
/* "2026-11-13 14:05" */
void rc_states_time_text(const rc_states_time *t, char *out, size_t capacity);

#endif
