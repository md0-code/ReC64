/* Snapshot files, media references, previews and quick-save slots. */
#include "states.h"
#include "../host/host.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

enum { MEDIA_HASHED = 1, MEDIA_PROTECTED = 2 };

/* --- the thumbnail ---------------------------------------------------------------------- */

int rc_states_thumb_make(const rc_frame *f, rc_states_thumb *t) {
    memset(t, 0, sizeof *t);
    if (!f->pixels || !f->palette || !f->colors || f->width < 4 || f->height < 4) return 0;
    uint32_t w = f->width / 4, h = f->height / 4;
    if (w > RC_STATES_THUMB_W) { h = h * RC_STATES_THUMB_W / w; w = RC_STATES_THUMB_W; }
    if (h > RC_STATES_THUMB_H) { w = w * RC_STATES_THUMB_H / h; h = RC_STATES_THUMB_H; }
    if (!w || !h) return 0;
    t->width = w;
    t->height = h;
    t->colors = f->colors > 256 ? 256 : f->colors;
    memcpy(t->palette, f->palette, t->colors * sizeof *t->palette);
    uint32_t count[256];
    for (uint32_t y = 0; y < h; ++y)
        for (uint32_t x = 0; x < w; ++x) {
            /* The block of the frame this pixel stands for: its commonest colour, or the second commonest when that covers a
             * fifth of the block, so that thin lines (the strokes of text) stay in sight rather than vanish into the paper. */
            const uint32_t x0 = x * f->width / w, x1 = (x + 1) * f->width / w, y0 = y * f->height / h, y1 = (y + 1) * f->height / h;
            uint32_t n = 0, first = 0, second = 0;
            memset(count, 0, sizeof count);
            for (uint32_t sy = y0; sy < y1; ++sy)
                for (uint32_t sx = x0; sx < x1; ++sx) {
                    const uint8_t i = f->pixels[(size_t)sy * f->pitch + sx];
                    ++count[i < t->colors ? i : 0];
                    ++n;
                }
            for (uint32_t i = 1; i < t->colors; ++i)
                if (count[i] > count[first]) first = i;
            if (first == 0) second = 1;
            for (uint32_t i = 0; i < t->colors; ++i)
                if (i != first && count[i] > count[second]) second = i;
            t->pixels[y * w + x] = (uint8_t)(count[second] * 5 >= n && count[second] ? second : first);
        }
    return 1;
}

/* --- writing and reading ------------------------------------------------------------------- */

int rc_states_write(const rc_machine *m, const rc_states_info *x, rc_state_writer *w) {
    rc_machine_save(m, w);
    if (x) {
        for (unsigned i = 0; i < RC_CONFIG_MEDIA; ++i) {
            const rc_states_media *md = &x->media[i];
            if (!md->present) continue;
            rc_sw_begin(w, "MDIA", i);
            rc_sw_str(w, md->path);
            rc_sw_u8(w, (uint8_t)((md->hashed ? MEDIA_HASHED : 0) | (md->write_protect ? MEDIA_PROTECTED : 0)));
            rc_sw_bytes(w, md->sha256, sizeof md->sha256);
            rc_sw_end(w);
        }
        if (x->has_thumb && x->thumb.width && x->thumb.height) {
            const rc_states_thumb *t = &x->thumb;
            rc_sw_begin(w, "THMB", 0);
            rc_sw_u16(w, (uint16_t)t->width);
            rc_sw_u16(w, (uint16_t)t->height);
            rc_sw_u16(w, (uint16_t)t->colors);
            for (uint32_t i = 0; i < t->colors; ++i) rc_sw_u32(w, t->palette[i]);
            rc_sw_packed(w, t->pixels, (size_t)t->width * t->height);
            rc_sw_end(w);
        }
        if (x->has_time) {
            rc_sw_begin(w, "TIME", 0);
            rc_sw_u16(w, x->time.year);
            rc_sw_u8(w, x->time.month);
            rc_sw_u8(w, x->time.day);
            rc_sw_u8(w, x->time.hour);
            rc_sw_u8(w, x->time.minute);
            rc_sw_u8(w, x->time.second);
            rc_sw_end(w);
        }
        if (x->version[0]) {
            rc_sw_begin(w, "VERS", 0);
            rc_sw_str(w, x->version);
            rc_sw_end(w);
        }
    }
    return !w->failed;
}

int rc_states_finish(rc_state_writer *w){
    uint8_t hash[32];if(w->failed)return 0;rc_sha256(w->data,w->size,hash);
    rc_sw_begin(w,"SHA2",0);rc_sw_bytes(w,hash,sizeof hash);rc_sw_end(w);return rc_sw_finish(w);
}

int rc_states_read(const uint8_t *data, size_t size, rc_states_info *info, rc_state_reader *r, char *error, size_t capacity) {
    memset(info, 0, sizeof *info);
    if (!rc_sr_open(r, data, size, error, capacity)) return 0;
    if(rc_sr_chunk(r,"SHA2",0)){
        const size_t at=r->at-12;uint8_t expected[32];rc_sha256(data,at,expected);
        if(rc_sr_count(r,"SHA2")!=1||r->end-r->at!=32||r->end+12!=size||memcmp(expected,data+r->at,32)){
            snprintf(error,capacity,"Snapshot checksum does not match");return 0;
        }
    }
    snprintf(info->family, sizeof info->family, "%s", r->family);
    snprintf(info->model, sizeof info->model, "%s", r->model);
    for (unsigned i = 0; i < RC_CONFIG_MEDIA; ++i) {
        rc_states_media *md = &info->media[i];
        if (!rc_sr_chunk(r, "MDIA", i)) continue;
        rc_sr_str(r, md->path, sizeof md->path);
        if(!rc_sr_ok(r)||r->end-r->at<33){snprintf(error,capacity,"Invalid snapshot media reference");return 0;}
        const uint8_t flags = rc_sr_u8(r);
        if(flags&~3u){snprintf(error,capacity,"Invalid snapshot media flags");return 0;}
        rc_sr_bytes(r, md->sha256, sizeof md->sha256);
        md->present = md->path[0] != 0;
        md->hashed = (flags & MEDIA_HASHED) != 0;
        md->write_protect = (flags & MEDIA_PROTECTED) != 0;
    }
    if (rc_sr_chunk(r, "THMB", 0)) {
        rc_states_thumb *t = &info->thumb;
        t->width = rc_sr_u16(r);
        t->height = rc_sr_u16(r);
        t->colors = rc_sr_u16(r);
        if (t->width && t->height && t->width <= RC_STATES_THUMB_W && t->height <= RC_STATES_THUMB_H && t->colors && t->colors <= 256) {
            for (uint32_t i = 0; i < t->colors; ++i) t->palette[i] = rc_sr_u32(r) & 0xFFFFFFu;
            rc_sr_packed(r, t->pixels, (size_t)t->width * t->height);
            info->has_thumb = rc_sr_ok(r);
            for (size_t i = 0; info->has_thumb && i < (size_t)t->width * t->height; ++i)
                if (t->pixels[i] >= t->colors) t->pixels[i] = 0;
        }
        r->failed = 0;                  /* a damaged thumbnail is no thumbnail; the machine's chunks decide the rest */
        if (!info->has_thumb) memset(t, 0, sizeof *t);
    }
    if (rc_sr_chunk(r, "TIME", 0)) {
        info->time.year = rc_sr_u16(r);
        info->time.month = rc_sr_u8(r);
        info->time.day = rc_sr_u8(r);
        info->time.hour = rc_sr_u8(r);
        info->time.minute = rc_sr_u8(r);
        info->time.second = rc_sr_u8(r);
        info->has_time = info->time.month >= 1 && info->time.month <= 12 && info->time.day >= 1 && info->time.day <= 31 &&
                         info->time.hour < 24 && info->time.minute < 60 && info->time.second < 61;
    }
    if (rc_sr_chunk(r, "VERS", 0)) rc_sr_str(r, info->version, sizeof info->version);
    r->failed = 0;
    return 1;
}

/* --- files and slots ---------------------------------------------------------------------- */

void rc_states_slot_path(const char *config_source, const char *model, unsigned slot, char *out, size_t capacity) {
    if(!capacity)return;out[0]=0;if(slot<1||slot>RC_STATES_SLOTS)return;
    const char sep = rc_host_separator();
    size_t folder = 0;
    for (size_t i = 0; config_source[i]; ++i)
        if (config_source[i] == '/' || config_source[i] == sep) folder = i + 1;
    const int n=snprintf(out, capacity, "%.*sstates%c%s%cslot-%u.rcs", (int)folder, config_source, sep, model, sep, slot);if(n<0||(size_t)n>=capacity)out[0]=0;
}

int rc_states_save_file(const char *path, const uint8_t *data, size_t size, char *error, size_t capacity) {
    char folder[RC_HOST_PATH];
    if(!path[0]||strlen(path)>=sizeof folder){snprintf(error,capacity,"Snapshot path is too long or empty");return 0;}
    snprintf(folder, sizeof folder, "%s", path);
    char *cut = NULL;
    for (char *c = folder; *c; ++c) if (*c == '/' || *c == rc_host_separator()) cut = c;
    if (cut && cut != folder) {
        *cut = 0;
        if (!rc_host_make_folders(folder)) { snprintf(error, capacity, "Cannot make the folder %s", folder); return 0; }
    }
    return rc_host_write_file(path, data, size, error, capacity);
}

int rc_states_load_file(const char *path, uint8_t **data, size_t *size, char *error, size_t capacity) {
    int writable = 0;
    return rc_host_read_file(path, 256u*1024u*1024u, data, size, &writable, error, capacity);
}

int rc_states_slot_info(const char *path, rc_states_info *info) {
    uint8_t *data = NULL;
    size_t size = 0;
    char error[256];
    rc_state_reader r;
    memset(info, 0, sizeof *info);
    if (!rc_host_file_exists(path) || !rc_states_load_file(path, &data, &size, error, sizeof error)) return 0;
    const int ok = rc_states_read(data, size, info, &r, error, sizeof error);
    free(data);
    return ok;
}

void rc_states_time_text(const rc_states_time *t, char *out, size_t capacity) {
    snprintf(out, capacity, "%04u-%02u-%02u %02u:%02u", (unsigned)t->year, (unsigned)t->month, (unsigned)t->day, (unsigned)t->hour,
             (unsigned)t->minute);
}


static int recover_media(const rc_media *image, unsigned slot, const char *base, char *result, size_t cap) {
    if (!image || !image->dirty || image->in_place) return 0;
    char path[RC_HOST_PATH], extension[32], why[256];
    rc_media_extension(image->path, extension, sizeof extension);
    for (unsigned n = 1; n <= 9999; ++n) {
        const int length = snprintf(path, sizeof path, "%s.recovery-%u-%u%s%s", base, slot, n, extension[0] ? "." : "", extension);
        if (length < 0 || (size_t)length >= sizeof path) break;
        if (!rc_host_file_create(path, 0, 0, why, sizeof why)) {
            if (rc_host_file_exists(path)) continue;
            break;
        }
        if (rc_host_write_file(path, image->data, image->size, why, sizeof why)) {
            snprintf(result, cap, "Saved a recovery copy to %s", path);
            return 1;
        }
        rc_host_delete_file(path);
        break;
    }
    return 0;
}

int rc_states_finish_media(rc_machine *m, unsigned slot, rc_media *owned, const char *base, char *error, size_t capacity) {
    const rc_family *family = rc_machine_family(m);
    rc_media *mounted = slot < family->media_count ? rc_machine_media(m, slot) : NULL;
    rc_media *companion = rc_machine_media_companion(m, slot);
    int ok = 1;
    if (mounted && (family->media[slot].kind == RC_MEDIA_FLOPPY || family->media[slot].kind == RC_MEDIA_CART))
        ok = rc_machine_control(m, slot, RC_MEDIA_FLUSH, 0);
    char why[256] = "Cannot flush mounted media";
    if (ok && mounted && !rc_media_save(mounted)) {
        snprintf(why, sizeof why, "%s", mounted->error);
        ok = 0;
    }
    if (owned && owned != mounted && !rc_media_save(owned)) {
        snprintf(why, sizeof why, "%s", owned->error);
        ok = 0;
    }
    if (!ok) {
        rc_media *images[] = {mounted, owned != mounted ? owned : NULL, companion};
        snprintf(error, capacity, "%s", why);
        for (unsigned i = 0; i < sizeof images / sizeof images[0]; ++i) {
            char saved[RC_HOST_PATH + 32];
            if (recover_media(images[i], slot, base, saved, sizeof saved)) {
                size_t used = strlen(error);
                if (used < capacity) snprintf(error + used, capacity - used, ". %s", saved);
            }
        }
    }
    return ok;
}
