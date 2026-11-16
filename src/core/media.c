/* Media files (media.h).  */
#include "media.h"
#include "../host/host.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

rc_media *rc_media_open(const char *path, int write_protect, char *error, size_t capacity) {
    rc_media *m = calloc(1, sizeof *m);
    if (!m) { if (capacity) snprintf(error, capacity, "Out of memory"); return NULL; }
    if (strlen(path) >= sizeof m->path) { if (capacity) snprintf(error, capacity, "Path is too long"); free(m); return NULL; }
    snprintf(m->path, sizeof m->path, "%s", path);
    if (!rc_host_read_file(path, RC_MEDIA_LIMIT, &m->data, &m->size, &m->writable, error, capacity)) { free(m); return NULL; }
    if (write_protect) m->writable = 0;
    return m;
}

rc_media *rc_media_open_in_place(const char *path, int write_protect, char *error, size_t capacity) {
    rc_media *m = calloc(1, sizeof *m);
    if (!m) { if (capacity) snprintf(error, capacity, "Out of memory"); return NULL; }
    if (strlen(path) >= sizeof m->path) { if (capacity) snprintf(error, capacity, "Path is too long"); free(m); return NULL; }
    snprintf(m->path, sizeof m->path, "%s", path);
    rc_host_file *f = rc_host_file_open(path, &m->writable, error, capacity);
    if (!f) { free(m); return NULL; }
    rc_host_file_close(f);
    if (write_protect) m->writable = 0;
    m->in_place = 1;
    return m;
}

rc_media *rc_media_from(const char *path, const uint8_t *data, size_t size) {
    rc_media *m = calloc(1, sizeof *m);
    if (!m) return NULL;
    snprintf(m->path, sizeof m->path, "%s", path ? path : "");
    m->data = malloc(size ? size : 1);
    if (!m->data) { free(m); return NULL; }
    if (size) memcpy(m->data, data, size);
    m->size = size;
    return m;
}

int rc_media_resize(rc_media *m, size_t size) {
    uint8_t *data = realloc(m->data, size ? size : 1);
    if (!data) return 0;
    if (size > m->size) memset(data + m->size, 0, size - m->size);
    m->data = data;
    m->size = size;
    return 1;
}

int rc_media_save(rc_media *m) {
    if (!m->dirty) return 1;
    if (!m->writable || !m->path[0]) { snprintf(m->error, sizeof m->error, "The file is read-only"); return 0; }
    if (!rc_host_write_file(m->path, m->data, m->size, m->error, sizeof m->error)) return 0;
    m->dirty = 0;
    return 1;
}

void rc_media_close(rc_media *m) {
    if (!m) return;
    free(m->data);
    free(m);
}

void rc_media_extension(const char *path, char *out, size_t capacity) {
    const char *dot = NULL;
    for (const char *p = path; *p; ++p) {
        if (*p == '.') dot = p;
        else if (*p == '/' || *p == '\\') dot = NULL;
    }
    size_t n = 0;
    if (dot)
        for (const char *p = dot + 1; *p && n + 1 < capacity; ++p) out[n++] = (char)(*p >= 'A' && *p <= 'Z' ? *p - 'A' + 'a' : *p);
    if (capacity) out[n] = 0;
}

int rc_media_extension_in(const char *path, const char *list) {
    char ext[16];
    rc_media_extension(path, ext, sizeof ext);
    const size_t n = strlen(ext);
    if (!n || !list) return 0;
    for (const char *p = list; *p;) {
        const char *end = strchr(p, ',');
        const size_t len = end ? (size_t)(end - p) : strlen(p);
        if (len == n && !memcmp(p, ext, n)) return 1;
        p += len + (end ? 1 : 0);
    }
    return 0;
}
