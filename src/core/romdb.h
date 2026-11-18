/* ROM identification. Every file in a roms folder is known by the SHA-256
 * of its content, whatever its name. A board asks for its ROMs by key. */

#ifndef RC_ROMDB_H
#define RC_ROMDB_H

#include <stddef.h>
#include <stdint.h>

#define RC_ROM_PATH 1024

typedef struct rc_known_rom {
    const char *key;           /* "test", "basic" */
    const char *name;          /* "C64 BASIC" */
    uint32_t size;
    const char *sha256;        /* lower-case hex */
} rc_known_rom;

size_t rc_romdb_count(void);
const rc_known_rom *rc_romdb_entry(size_t index);
/* The known ROM with this content, or NULL. */
const rc_known_rom *rc_romdb_identify(const uint8_t *data, size_t size);
const rc_known_rom *rc_romdb_find(const char *key);

/* SHA-256 (FIPS 180-4) of data, as 32 bytes and as lower-case hex. */
void rc_sha256(const uint8_t *data, size_t size, uint8_t digest[32]);
void rc_sha256_hex(const uint8_t *data, size_t size, char hex[65]);

/* The files of the roms folders, read once at start and identified by
 * content. A recognised file's content is kept in memory. */
typedef struct rc_rom_file {
    char path[RC_ROM_PATH];
    const char *name;          /* the file name, inside path */
    uint64_t size;
    char sha256[65];           /* empty for files too large to be a ROM */
    const rc_known_rom *rom;   /* NULL: not a known ROM */
    uint8_t *data;             /* a known ROM's content */
    int duplicate;             /* index of the file used for the same ROM, or -1 */
} rc_rom_file;

typedef struct rc_rom_index {
    rc_rom_file *files;
    size_t count, capacity;
    char folders[3][RC_ROM_PATH];   /* the folders read (those that exist), each with its separator */
    size_t folder_count;
} rc_rom_index;

void rc_rom_index_init(rc_rom_index *index);
/* Reads every file of each folder (missing folders are skipped; the same
 * folder named twice is read once), the files of a folder in name order.
 * Of files holding the same ROM the first found is used. 0 when out of
 * memory. */
int rc_rom_index_scan(rc_rom_index *index, const char *const *folders, size_t count);
/* Adds a file's content as if found under `name` (tests); 0 when out of
 * memory. */
int rc_rom_index_add(rc_rom_index *index, const char *name, const uint8_t *data, size_t size);
void rc_rom_index_free(rc_rom_index *index);
/* The file used for a known ROM, or NULL when it was not found. */
const rc_rom_file *rc_rom_index_find(const rc_rom_index *index, const rc_known_rom *rom);
/* What a file was recognised as, in words. */
const char *rc_rom_file_describe(const rc_rom_index *index, const rc_rom_file *file, char *buffer, size_t capacity);

#endif /* RC_ROMDB_H */
