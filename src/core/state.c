/* Saved states (state.h).  */
#include "state.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static const char MAGIC[8] = {'R', 'E', 'C', '6', '4', 'S', 'T', '1'};
static const char END[4] = {'E', 'N', 'D', ' '};

/* --- writing ---------------------------------------------------------------------- */

void rc_sw_init(rc_state_writer *w) { memset(w, 0, sizeof *w); }
void rc_sw_free(rc_state_writer *w) { free(w->data); memset(w, 0, sizeof *w); }

static uint8_t *room(rc_state_writer *w, size_t length) {
    if (w->failed) return NULL;
    if (length > SIZE_MAX - w->size) { w->failed = 1; return NULL; }
    if (w->size + length > w->capacity) {
        size_t capacity = w->capacity ? w->capacity * 2 : 65536;
        while (capacity < w->size + length) {
            if (capacity > SIZE_MAX / 2) { capacity = w->size + length; break; }
            capacity *= 2;
        }
        uint8_t *data = realloc(w->data, capacity);
        if (!data) { w->failed = 1; return NULL; }
        w->data = data;
        w->capacity = capacity;
    }
    uint8_t *p = w->data + w->size;
    w->size += length;
    return p;
}

void rc_sw_bytes(rc_state_writer *w, const void *data, size_t length) {
    uint8_t *p = room(w, length);
    if (p && length) memcpy(p, data, length);
}
void rc_sw_u8(rc_state_writer *w, uint8_t v) { rc_sw_bytes(w, &v, 1); }
void rc_sw_u16(rc_state_writer *w, uint16_t v) { const uint8_t b[2] = {(uint8_t)v, (uint8_t)(v >> 8)}; rc_sw_bytes(w, b, 2); }
void rc_sw_u32(rc_state_writer *w, uint32_t v) {
    const uint8_t b[4] = {(uint8_t)v, (uint8_t)(v >> 8), (uint8_t)(v >> 16), (uint8_t)(v >> 24)};
    rc_sw_bytes(w, b, 4);
}
void rc_sw_u64(rc_state_writer *w, uint64_t v) { rc_sw_u32(w, (uint32_t)v); rc_sw_u32(w, (uint32_t)(v >> 32)); }
void rc_sw_str(rc_state_writer *w, const char *text) {
    const size_t n = strlen(text) > 65535 ? 65535 : strlen(text);
    rc_sw_u16(w, (uint16_t)n);
    rc_sw_bytes(w, text, n);
}

void rc_sw_header(rc_state_writer *w, const char *family, const char *model, const uint8_t *hashes, uint32_t count) {
    rc_sw_bytes(w, MAGIC, sizeof MAGIC);
    rc_sw_u32(w, RC_STATE_VERSION);
    rc_sw_str(w, family);
    rc_sw_str(w, model);
    rc_sw_u32(w, count);
    rc_sw_bytes(w, hashes, (size_t)count * 32u);
}

void rc_sw_begin(rc_state_writer *w, const char tag[4], uint32_t instance) {
    rc_sw_bytes(w, tag, 4);
    rc_sw_u32(w, instance);
    w->chunk = w->size;
    rc_sw_u32(w, 0);
}

void rc_sw_end(rc_state_writer *w) {
    if (w->failed || !w->chunk) return;
    const uint32_t length = (uint32_t)(w->size - w->chunk - 4u);
    uint8_t *p = w->data + w->chunk;
    p[0] = (uint8_t)length; p[1] = (uint8_t)(length >> 8); p[2] = (uint8_t)(length >> 16); p[3] = (uint8_t)(length >> 24);
    w->chunk = 0;
}

/* Runs: a control byte c, then for c < 128 c + 1 literal bytes, else one
 * byte repeated c - 125 times (3 to 130). */
void rc_sw_packed(rc_state_writer *w, const uint8_t *data, size_t length) {
    rc_sw_u32(w, (uint32_t)length);
    size_t i = 0;
    while (i < length) {
        size_t run = 1;
        while (i + run < length && run < 130 && data[i + run] == data[i]) ++run;
        if (run >= 3) {
            rc_sw_u8(w, (uint8_t)(run + 125u));
            rc_sw_u8(w, data[i]);
            i += run;
            continue;
        }
        size_t n = 0;                           /* literals up to the next run of three */
        while (i + n < length && n < 128 &&
               !(i + n + 2 < length && data[i + n] == data[i + n + 1] && data[i + n] == data[i + n + 2])) ++n;
        rc_sw_u8(w, (uint8_t)(n - 1u));
        rc_sw_bytes(w, data + i, n);
        i += n;
    }
}

int rc_sw_finish(rc_state_writer *w) {
    rc_sw_bytes(w, END, 4);
    rc_sw_u32(w, 0);
    rc_sw_u32(w, 0);
    return !w->failed;
}

/* --- reading ---------------------------------------------------------------------- */

static uint32_t get32(const uint8_t *p) { return p[0] | (uint32_t)p[1] << 8 | (uint32_t)p[2] << 16 | (uint32_t)p[3] << 24; }

static int refuse(char *error, size_t capacity, const char *why) {
    if (capacity) snprintf(error, capacity, "%s", why);
    return 0;
}

/* A header string at *at; 0 when it does not fit the state or the buffer. */
static int header_str(const uint8_t *data, size_t size, size_t *at, char *out) {
    if (*at + 2 > size) return 0;
    const size_t n = data[*at] | (size_t)data[*at + 1] << 8;
    if (n >= RC_STATE_ID || *at + 2 + n > size) return 0;
    memcpy(out, data + *at + 2, n);
    out[n] = 0;
    *at += 2 + n;
    return 1;
}

int rc_sr_open(rc_state_reader *r, const uint8_t *data, size_t size, char *error, size_t capacity) {
    memset(r, 0, sizeof *r);
    if (size < 12 || memcmp(data, MAGIC, sizeof MAGIC)) return refuse(error, capacity, "This is not a ReC64 state");
    if (get32(data + 8) == 0) return refuse(error, capacity, "Invalid state version");
    if (get32(data + 8) > RC_STATE_VERSION) return refuse(error, capacity, "The state was saved by a newer ReC64");
    size_t at = 12;
    if (!header_str(data, size, &at, r->family) || !header_str(data, size, &at, r->model) || at + 4 > size)
        return refuse(error, capacity, "The state is damaged");
    r->hash_count = get32(data + at);
    at += 4;
    if (r->hash_count > 64 || at + (size_t)r->hash_count * 32u > size) return refuse(error, capacity, "The state is damaged");
    r->hashes = data + at;
    at += (size_t)r->hash_count * 32u;
    /* Every chunk must lie inside the state and the end mark must be there. */
    for (size_t p = at;;) {
        if (p + 12 > size) return refuse(error, capacity, "The state is cut short");
        const size_t length = get32(data + p + 8);
        if (!memcmp(data + p, END, 4)) {
            if (length || get32(data + p + 4) || p + 12 != size) return refuse(error, capacity, "Invalid state end marker");
            break;
        }
        if (length > size - p - 12) return refuse(error, capacity, "The state is cut short");
        p += 12 + length;
    }
    r->data = data;
    r->size = size;
    r->body = at;
    return 1;
}

int rc_sr_chunk(rc_state_reader *r, const char tag[4], uint32_t instance) {
    r->at = r->end = 0;
    for (size_t p = r->body; memcmp(r->data + p, END, 4); p += 12 + get32(r->data + p + 8))
        if (!memcmp(r->data + p, tag, 4) && get32(r->data + p + 4) == instance) {
            r->at = p + 12;
            r->end = r->at + get32(r->data + p + 8);
            return 1;
        }
    return 0;
}

uint32_t rc_sr_count(const rc_state_reader *r, const char tag[4]) {
    uint32_t n = 0;
    for (size_t p = r->body; memcmp(r->data + p, END, 4); p += 12 + get32(r->data + p + 8)) n += !memcmp(r->data + p, tag, 4);
    return n;
}

/* Past the end of a chunk the fields read as zero: an older state's chunk is shorter. */
void rc_sr_bytes(rc_state_reader *r, void *data, size_t length) {
    const size_t have = r->end - r->at < length ? r->end - r->at : length;
    if (have) memcpy(data, r->data + r->at, have);
    if (have < length) memset((uint8_t *)data + have, 0, length - have);
    r->at += have;
}
uint8_t rc_sr_u8(rc_state_reader *r) { uint8_t b = 0; rc_sr_bytes(r, &b, 1); return b; }
uint16_t rc_sr_u16(rc_state_reader *r) { uint8_t b[2] = {0, 0}; rc_sr_bytes(r, b, 2); return (uint16_t)(b[0] | (uint16_t)b[1] << 8); }
uint32_t rc_sr_u32(rc_state_reader *r) { uint8_t b[4] = {0, 0, 0, 0}; rc_sr_bytes(r, b, 4); return get32(b); }
uint64_t rc_sr_u64(rc_state_reader *r) { const uint64_t lo = rc_sr_u32(r); return lo | (uint64_t)rc_sr_u32(r) << 32; }

void rc_sr_str(rc_state_reader *r, char *out, size_t capacity) {
    const size_t n = rc_sr_u16(r);
    const size_t keep = capacity ? (n < capacity - 1 ? n : capacity - 1) : 0;
    if (r->end - r->at < n) { r->failed = 1; if (capacity) out[0] = 0; r->at = r->end; return; }
    if (keep) memcpy(out, r->data + r->at, keep);
    if (capacity) out[keep] = 0;
    r->at += n;
}

void rc_sr_packed(rc_state_reader *r, uint8_t *data, size_t length) {
    if (r->end - r->at < 4 || rc_sr_u32(r) != length) { r->failed = 1; return; }
    size_t i = 0;
    while (i < length) {
        if (r->at >= r->end) { r->failed = 1; return; }
        const unsigned c = r->data[r->at++];
        const size_t n = c < 128 ? c + 1u : c - 125u;
        if (n > length - i || r->at + (c < 128 ? n : 1u) > r->end) { r->failed = 1; return; }
        if (c < 128) { memcpy(data + i, r->data + r->at, n); r->at += n; }
        else memset(data + i, r->data[r->at++], n);
        i += n;
    }
}

int rc_sr_ok(const rc_state_reader *r) { return !r->failed; }
