/* A media file as the machine holds it: the whole file in memory, with its
 * path and whether it may be written back. The formats (tapes, disk images,
 * cards, ROM sets) are read from these bytes by the modules of core/media/;
 * this is only the file. A machine that changes the content sets `dirty`,
 * and the shell writes the file back (rc_media_save) when the media is
 * ejected, at intervals and at exit. Large images used in place (cards) are
 * opened by their own module, not read through this: such a media only names
 * the file (in_place, rc_media_open_in_place).  */
#ifndef RC_MEDIA_H
#define RC_MEDIA_H

#include <stddef.h>
#include <stdint.h>

#define RC_MEDIA_PATH 1024
#define RC_MEDIA_LIMIT (64u * 1024u * 1024u)    /* nothing larger is read whole */

typedef struct rc_media {
    char path[RC_MEDIA_PATH];
    uint8_t *data;
    size_t size;
    int writable;                       /* the file can be written and is not write-protected */
    int dirty;                          /* the content differs from the file */
    int in_place;                       /* a card image: only the path is held, the machine opens the file itself and uses it in place */
    char error[128];                    /* why the last save failed */
} rc_media;

/* Reads the file; NULL with the reason. `write_protect` makes it read-only
 * whatever the file allows. */
rc_media *rc_media_open(const char *path, int write_protect, char *error, size_t capacity);
/* The media of a file used in place (a card image: large, read and written a sector at a time by media/card.h): nothing is read,
 * `data` is NULL and `size` 0; the file must exist. `writable` says whether it may be written. NULL with the reason. */
rc_media *rc_media_open_in_place(const char *path, int write_protect, char *error, size_t capacity);
/* A media made of bytes (copied), for checks and for files made in memory. */
rc_media *rc_media_from(const char *path, const uint8_t *data, size_t size);
/* Makes the content `size` bytes (new bytes are zero); 0 when out of memory. */
int rc_media_resize(rc_media *m, size_t size);
/* Writes a changed, writable media back; 1 when the file is up to date. */
int rc_media_save(rc_media *m);
void rc_media_close(rc_media *m);
/* The file name's ending in lower case without its dot ("tap"; "" for none). */
void rc_media_extension(const char *path, char *out, size_t capacity);
/* 1 when the ending is in a list such as "d64,t64,tap". */
int rc_media_extension_in(const char *path, const char *list);

#endif /* RC_MEDIA_H */
