/* Saved states (PLAN.md sections 4 and 5): ReC64's own format, the same for
 * every machine. A state is a header and a list of chunks:
 *
 *   "REC64ST1"                      magic
 *   u32  format version             RC_STATE_VERSION
 *   str  family id, str model id    ("c64", "c64-pal"); str is a u16 length and the bytes
 *   u32  count, then count x 32     the SHA-256 of each ROM the machine runs
 *   chunks                          tag (4 bytes), u32 instance, u32 length, the bytes
 *   "END ", 0, 0
 *
 * A chunk holds one chip's or one memory block's state (tag "CPU ", "VIC2",
 * "RAM "; the instance tells two chips of a kind apart). A reader skips the
 * chunks it does not know and finds the ones it wants by tag and instance,
 * whatever their order, so a later version may add chunks, and a chip may
 * add fields at the end of its chunk (a reader that runs off the end of an
 * older chunk reads zeros). Media are not in a state: a state names them by
 * path and hash ("MDIA" chunks, the shell's), and the shell puts them back.
 *
 * All numbers are little-endian. Memory is run-length packed (rc_sw_packed).
 * The writer grows a buffer in memory, so the same serialisation feeds the
 * state files and in-memory snapshots. No OS call here: the shell reads and
 * writes the files.  */
#ifndef RC_STATE_H
#define RC_STATE_H

#include <stddef.h>
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif

#define RC_STATE_VERSION 1u
#define RC_STATE_ID 32                  /* a family or model id with its end */

/* --- writing ---------------------------------------------------------------------- */

typedef struct rc_state_writer {
    uint8_t *data;
    size_t size, capacity;
    size_t chunk;                       /* where the open chunk's length field is; 0: none open */
    int failed;                         /* out of memory: everything after is dropped */
} rc_state_writer;

void rc_sw_init(rc_state_writer *w);
void rc_sw_free(rc_state_writer *w);
/* The header: what machine this is a state of. `hashes` is count x 32 bytes. */
void rc_sw_header(rc_state_writer *w, const char *family, const char *model, const uint8_t *hashes, uint32_t count);
/* A chunk: begin, the fields, end. Chunks do not nest. */
void rc_sw_begin(rc_state_writer *w, const char tag[4], uint32_t instance);
void rc_sw_end(rc_state_writer *w);
void rc_sw_u8(rc_state_writer *w, uint8_t v);
void rc_sw_u16(rc_state_writer *w, uint16_t v);
void rc_sw_u32(rc_state_writer *w, uint32_t v);
void rc_sw_u64(rc_state_writer *w, uint64_t v);
void rc_sw_bytes(rc_state_writer *w, const void *data, size_t length);
void rc_sw_str(rc_state_writer *w, const char *text);
/* `length` bytes of memory, run-length packed: a u32 length, then runs. */
void rc_sw_packed(rc_state_writer *w, const uint8_t *data, size_t length);
/* The end mark. 1 when the whole state is in w->data (w->size bytes). */
int rc_sw_finish(rc_state_writer *w);

/* --- reading ---------------------------------------------------------------------- */

typedef struct rc_state_reader {
    const uint8_t *data;
    size_t size;
    size_t body;                        /* where the chunks start */
    size_t at, end;                     /* the open chunk's read position and end */
    int failed;                         /* a field was read that the state does not have right */
    char family[RC_STATE_ID], model[RC_STATE_ID];
    const uint8_t *hashes;
    uint32_t hash_count;
} rc_state_reader;

/* Reads the header; 0 with the reason when this is not a state ReC64 reads. */
int rc_sr_open(rc_state_reader *r, const uint8_t *data, size_t size, char *error, size_t capacity);
/* Opens the chunk with this tag and instance for the calls below; 0 when the
 * state has none (the fields then read as zero). */
int rc_sr_chunk(rc_state_reader *r, const char tag[4], uint32_t instance);
/* How many chunks carry this tag (the instances are not always 0..n-1). */
uint32_t rc_sr_count(const rc_state_reader *r, const char tag[4]);
uint8_t  rc_sr_u8(rc_state_reader *r);
uint16_t rc_sr_u16(rc_state_reader *r);
uint32_t rc_sr_u32(rc_state_reader *r);
uint64_t rc_sr_u64(rc_state_reader *r);
void rc_sr_bytes(rc_state_reader *r, void *data, size_t length);
/* A string of at most capacity - 1 bytes (a longer one is cut). */
void rc_sr_str(rc_state_reader *r, char *out, size_t capacity);
/* Memory packed by rc_sw_packed: exactly `length` bytes or the reader fails. */
void rc_sr_packed(rc_state_reader *r, uint8_t *data, size_t length);
/* 1 while every field read so far was there in the right shape. */
int rc_sr_ok(const rc_state_reader *r);

#ifdef __cplusplus
}
#endif
#endif /* RC_STATE_H */
