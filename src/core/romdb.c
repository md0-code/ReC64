/* ROM identification (romdb.h): SHA-256 as FIPS 180-4 specifies it, the
 * table of known images, and the index of the roms folders. ReC64's own
 */
#include "romdb.h"
#include "../host/host.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* The images tools/fetch_roms.
 * cores' lock files record. */
/* Filled from the pinned firmware data in docs/sources.lock.json. */
static const rc_known_rom ROMS[] = {
    {"kernal", "C64 KERNAL 901227-03", 8192u, "83c60d47047d7beab8e5b7bf6f67f80daa088b7a6a27de0d7e016f6484042721"},
    {"basic", "C64 BASIC 901226-01", 8192u, "89878cea0a268734696de11c4bae593eaaa506465d2029d619c0e0cbccdfa62d"},
    {"chargen", "C64 character generator 901225-01", 4096u, "fd0d53b8480e86163ac98998976c72cc58d5dd8eb824ed7b829774e74213b420"},
    {"dos1541", "1541 DOS 325302-01 + 901229-05", 16384u, "d1d45afb46fd4e2b48d93ca367b889d75654a3b7acf73044e51f6d880c09369e"},
    {"dos1541ii", "1541-II DOS 251968-03", 16384u, "326c289c38753323d7e8167897447cf61ef35189d82eb8d75210ece949adda7c"},
};
#define ROM_COUNT (sizeof ROMS / sizeof ROMS[0])
/* Nothing larger is read whole. */
#define LARGEST 131072u

size_t rc_romdb_count(void) { return ROM_COUNT; }
const rc_known_rom *rc_romdb_entry(size_t i) { return i < ROM_COUNT ? &ROMS[i] : NULL; }

const rc_known_rom *rc_romdb_find(const char *key) {
    for (size_t i = 0; i < ROM_COUNT; ++i) if (!strcmp(ROMS[i].key, key)) return &ROMS[i];
    return NULL;
}
static const rc_known_rom *by_hash(const char *hex, uint64_t size) {
    for (size_t i = 0; i < ROM_COUNT; ++i) if (ROMS[i].size == size && !strcmp(ROMS[i].sha256, hex)) return &ROMS[i];
    return NULL;
}
const rc_known_rom *rc_romdb_identify(const uint8_t *data, size_t size) {
    char hex[65];
    if (size > LARGEST) return NULL;
    rc_sha256_hex(data, size, hex);
    return by_hash(hex, size);
}

/* --- SHA-256 (FIPS 180-4, sections 4.1.2, 4.2.2, 5.1.1, 6.2) ---------------------------- */

static const uint32_t K[64] = {
    0x428a2f98u, 0x71374491u, 0xb5c0fbcfu, 0xe9b5dba5u, 0x3956c25bu, 0x59f111f1u, 0x923f82a4u, 0xab1c5ed5u,
    0xd807aa98u, 0x12835b01u, 0x243185beu, 0x550c7dc3u, 0x72be5d74u, 0x80deb1feu, 0x9bdc06a7u, 0xc19bf174u,
    0xe49b69c1u, 0xefbe4786u, 0x0fc19dc6u, 0x240ca1ccu, 0x2de92c6fu, 0x4a7484aau, 0x5cb0a9dcu, 0x76f988dau,
    0x983e5152u, 0xa831c66du, 0xb00327c8u, 0xbf597fc7u, 0xc6e00bf3u, 0xd5a79147u, 0x06ca6351u, 0x14292967u,
    0x27b70a85u, 0x2e1b2138u, 0x4d2c6dfcu, 0x53380d13u, 0x650a7354u, 0x766a0abbu, 0x81c2c92eu, 0x92722c85u,
    0xa2bfe8a1u, 0xa81a664bu, 0xc24b8b70u, 0xc76c51a3u, 0xd192e819u, 0xd6990624u, 0xf40e3585u, 0x106aa070u,
    0x19a4c116u, 0x1e376c08u, 0x2748774cu, 0x34b0bcb5u, 0x391c0cb3u, 0x4ed8aa4au, 0x5b9cca4fu, 0x682e6ff3u,
    0x748f82eeu, 0x78a5636fu, 0x84c87814u, 0x8cc70208u, 0x90befffau, 0xa4506cebu, 0xbef9a3f7u, 0xc67178f2u};

static uint32_t rotr(uint32_t x, unsigned n) { return (x >> n) | (x << (32u - n)); }

static void block(uint32_t h[8], const uint8_t *p) {
    uint32_t w[64];
    for (unsigned t = 0; t < 16; ++t)
        w[t] = (uint32_t)p[4 * t] << 24 | (uint32_t)p[4 * t + 1] << 16 | (uint32_t)p[4 * t + 2] << 8 | p[4 * t + 3];
    for (unsigned t = 16; t < 64; ++t) {
        const uint32_t s0 = rotr(w[t - 15], 7) ^ rotr(w[t - 15], 18) ^ (w[t - 15] >> 3);
        const uint32_t s1 = rotr(w[t - 2], 17) ^ rotr(w[t - 2], 19) ^ (w[t - 2] >> 10);
        w[t] = w[t - 16] + s0 + w[t - 7] + s1;
    }
    uint32_t a = h[0], b = h[1], c = h[2], d = h[3], e = h[4], f = h[5], g = h[6], k = h[7];
    for (unsigned t = 0; t < 64; ++t) {
        const uint32_t t1 = k + (rotr(e, 6) ^ rotr(e, 11) ^ rotr(e, 25)) + ((e & f) ^ (~e & g)) + K[t] + w[t];
        const uint32_t t2 = (rotr(a, 2) ^ rotr(a, 13) ^ rotr(a, 22)) + ((a & b) ^ (a & c) ^ (b & c));
        k = g; g = f; f = e; e = d + t1; d = c; c = b; b = a; a = t1 + t2;
    }
    h[0] += a; h[1] += b; h[2] += c; h[3] += d; h[4] += e; h[5] += f; h[6] += g; h[7] += k;
}

void rc_sha256(const uint8_t *data, size_t size, uint8_t digest[32]) {
    uint32_t h[8] = {0x6a09e667u, 0xbb67ae85u, 0x3c6ef372u, 0xa54ff53au, 0x510e527fu, 0x9b05688cu, 0x1f83d9abu, 0x5be0cd19u};
    size_t i = 0;
    for (; size - i >= 64; i += 64) block(h, data + i);
    /* The padding: a 1 bit, zeros, the length in bits as 64 big-endian bits. */
    uint8_t tail[128];
    const size_t rest = size - i, total = rest < 56 ? 64 : 128;
    memset(tail, 0, sizeof tail);
    if (rest) memcpy(tail, data + i, rest);
    tail[rest] = 0x80;
    const uint64_t bits = (uint64_t)size * 8u;
    for (unsigned b = 0; b < 8; ++b) tail[total - 1 - b] = (uint8_t)(bits >> (8 * b));
    block(h, tail);
    if (total == 128) block(h, tail + 64);
    for (unsigned b = 0; b < 8; ++b) {
        digest[4 * b] = (uint8_t)(h[b] >> 24);
        digest[4 * b + 1] = (uint8_t)(h[b] >> 16);
        digest[4 * b + 2] = (uint8_t)(h[b] >> 8);
        digest[4 * b + 3] = (uint8_t)h[b];
    }
}

void rc_sha256_hex(const uint8_t *data, size_t size, char hex[65]) {
    static const char digits[] = "0123456789abcdef";
    uint8_t d[32];
    rc_sha256(data, size, d);
    for (unsigned i = 0; i < 32; ++i) {
        hex[2 * i] = digits[d[i] >> 4];
        hex[2 * i + 1] = digits[d[i] & 15];
    }
    hex[64] = 0;
}

/* --- the index ------------------------------------------------------------------- */

void rc_rom_index_init(rc_rom_index *ix) { memset(ix, 0, sizeof *ix); }

static rc_rom_file *new_file(rc_rom_index *ix) {
    if (ix->count == ix->capacity) {
        const size_t capacity = ix->capacity ? ix->capacity * 2 : 16;
        rc_rom_file *files = realloc(ix->files, capacity * sizeof *files);
        if (!files) return NULL;
        ix->files = files;
        ix->capacity = capacity;
    }
    rc_rom_file *f = &ix->files[ix->count];
    memset(f, 0, sizeof *f);
    f->duplicate = -1;
    return f;
}

/* Identifies the newest file and settles which of the same ROM is used. */
static void settle(rc_rom_index *ix, rc_rom_file *f, const uint8_t *data) {
    if (f->size <= LARGEST && data) {
        rc_sha256_hex(data, (size_t)f->size, f->sha256);
        f->rom = by_hash(f->sha256, f->size);
    }
    if (!f->rom) return;
    f->data = malloc((size_t)f->size);
    if (f->data) memcpy(f->data, data, (size_t)f->size);
    else f->rom = NULL;
    const size_t self = (size_t)(f - ix->files);
    for (size_t i = 0; f->rom && i < self; ++i) {
        rc_rom_file *o = &ix->files[i];
        if (o->rom != f->rom || o->duplicate >= 0) continue;
        f->duplicate = (int)i;     /* the first found is used */
        break;
    }
}

int rc_rom_index_add(rc_rom_index *ix, const char *name, const uint8_t *data, size_t size) {
    rc_rom_file *f = new_file(ix);
    if (!f) return 0;
    snprintf(f->path, sizeof f->path, "%s", name);
    f->name = f->path;
    f->size = size;
    ix->count++;
    settle(ix, f, data);
    return 1;
}

typedef struct names { char (*list)[256]; size_t count, capacity; int failed; } names;
static int collect(void *ctx, const char *name) {
    names *n = ctx;
    if (strlen(name) >= 256) return 1;
    if (n->count == n->capacity) {
        const size_t capacity = n->capacity ? n->capacity * 2 : 32;
        char (*list)[256] = realloc(n->list, capacity * sizeof *list);
        if (!list) { n->failed = 1; return 0; }
        n->list = list;
        n->capacity = capacity;
    }
    snprintf(n->list[n->count++], 256, "%s", name);
    return 1;
}
static int by_name(const void *a, const void *b) { return strcmp((const char *)a, (const char *)b); }

/* 1 read, 0 out of memory, -1 no such folder. */
static int scan_folder(rc_rom_index *ix, const char *folder) {
    names n = {NULL, 0, 0, 0};
    if (!rc_host_list_files(folder, collect, &n)) { free(n.list); return -1; }
    if (n.failed) { free(n.list); return 0; }
    if (n.count) qsort(n.list, n.count, sizeof *n.list, by_name);
    uint8_t *buffer = malloc(LARGEST);
    int ok = buffer != NULL;
    for (size_t i = 0; ok && i < n.count; ++i) {
        rc_rom_file *f = new_file(ix);
        if (!f) { ok = 0; break; }
        if (snprintf(f->path, sizeof f->path, "%s%s", folder, n.list[i]) >= (int)sizeof f->path) continue;
        f->name = f->path + strlen(folder);
        FILE *file = fopen(f->path, "rb");
        if (!file) continue;
        long size = -1;
        if (!fseek(file, 0, SEEK_END)) size = ftell(file);
        int whole = 0;
        if (size >= 0) {
            f->size = (uint64_t)size;
            whole = size <= (long)LARGEST && !fseek(file, 0, SEEK_SET) && fread(buffer, 1, (size_t)size, file) == (size_t)size;
        }
        fclose(file);
        if (size < 0) continue;
        ix->count++;
        settle(ix, f, whole ? buffer : NULL);
    }
    free(buffer);
    free(n.list);
    return ok;
}

int rc_rom_index_scan(rc_rom_index *ix, const char *const *folders, size_t count) {
    for (size_t k = 0; k < count && ix->folder_count < 3; ++k) {
        if (!folders[k] || !folders[k][0]) continue;
        int seen = 0;
        for (size_t j = 0; j < ix->folder_count; ++j) seen |= rc_host_same_folder(ix->folders[j], folders[k]);
        if (seen) continue;
        char *folder = ix->folders[ix->folder_count++];
        const size_t n = strlen(folders[k]);
        const int separated = folders[k][n - 1] == '/' || folders[k][n - 1] == '\\';
        snprintf(folder, RC_ROM_PATH, "%s%s", folders[k], separated ? "" : (char[2]){rc_host_separator(), 0});
        const int read = scan_folder(ix, folder);
        if (!read) return 0;
        if (read < 0) ix->folder_count--;      /* missing: not one of the folders */
    }
    return 1;
}

void rc_rom_index_free(rc_rom_index *ix) {
    for (size_t i = 0; i < ix->count; ++i) free(ix->files[i].data);
    free(ix->files);
    memset(ix, 0, sizeof *ix);
}

const rc_rom_file *rc_rom_index_find(const rc_rom_index *ix, const rc_known_rom *rom) {
    if (!ix || !rom) return NULL;
    for (size_t i = 0; i < ix->count; ++i)
        if (ix->files[i].rom == rom && ix->files[i].duplicate < 0 && ix->files[i].data) return &ix->files[i];
    return NULL;
}

const char *rc_rom_file_describe(const rc_rom_index *ix, const rc_rom_file *f, char *buffer, size_t capacity) {
    if (!f->rom) {
        if (!f->sha256[0]) snprintf(buffer, capacity, "unknown (%llu bytes, too large for a ROM)", (unsigned long long)f->size);
        else snprintf(buffer, capacity, "unknown (%llu bytes, SHA-256 %.16s...)", (unsigned long long)f->size, f->sha256);
    } else if (f->duplicate >= 0)
        snprintf(buffer, capacity, "%s, a copy of %s (not used)", f->rom->name, ix->files[f->duplicate].name);
    else snprintf(buffer, capacity, "%s", f->rom->name);
    return buffer;
}
