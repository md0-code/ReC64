/* Editing rec64.yaml in place (the setup screen's save): the file is kept
 * as its lines, and an edit changes only the lines of the key it touches,
 * so comments, blank lines, the order of sections and keys and quoting stay
 * as they were. Copied  (its own code) for the block style the
 * template uses (indented mappings and sequences, flow collections on one
 * or more lines, comments); a flow mapping that an edit reaches into is
 * rewritten in block style. ReC64 reads the result with libyaml as usual
 * (config.c). */

#ifndef RC_YAMLEDIT_H
#define RC_YAMLEDIT_H

#include <stddef.h>

typedef struct rc_ydoc {
    char **lines;              /* without line ends */
    size_t count, capacity;
    int crlf;                  /* the file's lines end in CR LF */
} rc_ydoc;

/* The text as lines (any of LF, CR LF). 0 when out of memory. */
int rc_ydoc_parse(rc_ydoc *doc, const char *text, size_t length);
int rc_ydoc_load(rc_ydoc *doc, const char *path);
/* The document as text (malloc'd, NUL-terminated); *length without the NUL. */
char *rc_ydoc_text(const rc_ydoc *doc, size_t *length);
/* Writes the file through a temporary file in its folder, replaced at once. */
int rc_ydoc_save(const rc_ydoc *doc, const char *path);
void rc_ydoc_free(rc_ydoc *doc);

/* A path is a list of mapping keys from the top (unquoted, as libyaml reads
 * them). */
int rc_ydoc_has(const rc_ydoc *doc, const char *const *path, size_t depth);
/* The scalar value of the key, unquoted; 0 when the key is missing or its
 * value is not a scalar on its line. */
int rc_ydoc_get(const rc_ydoc *doc, const char *const *path, size_t depth, char *out, size_t capacity);
/* Sets the key to a value written in YAML on its line (a scalar, quoted as
 * needed by the caller with rc_ydoc_quote, or a flow collection); creates the
 * key and the mappings above it when missing, uncommenting the template's
 * commented-out line for the key ("# key: ...") when there is one. */
int rc_ydoc_set(rc_ydoc *doc, const char *const *path, size_t depth, const char *value);
/* Rewrites the mapping at path, and every mapping inside it, in block
 * style (depth 0: every top-level mapping), so that each of their keys has
 * a line of its own. */
int rc_ydoc_expand(rc_ydoc *doc, const char *const *path, size_t depth);
/* Removes the key and its value; 1 when it was not there either. */
int rc_ydoc_remove(rc_ydoc *doc, const char *const *path, size_t depth);
/* The keys of the mapping at path (depth 0: the top), in order: up to max
 * names of up to 128 bytes. Returns how many. */
size_t rc_ydoc_keys(const rc_ydoc *doc, const char *const *path, size_t depth, char keys[][128], size_t max);

/* A string as a YAML scalar: plain when that reads back as the same string,
 * else in double quotes. */
const char *rc_ydoc_quote(const char *s, char *buffer, size_t capacity);
/* A path as a YAML scalar: single quotes keep backslashes as they are. */
const char *rc_ydoc_quote_path(const char *s, char *buffer, size_t capacity);

#endif /* RC_YAMLEDIT_H */
