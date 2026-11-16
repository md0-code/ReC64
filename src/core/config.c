/* rec64.yaml (config.h). libyaml handles the YAML syntax; this file
 * enforces the schema: unknown keys, duplicate keys, bad values and extra
 * documents are refused with a message naming what is wrong and its line.
 * 
 * schema is ReC64's own, and the machine's part of it comes from the
 * registry's tables (machine.h). */
#include "config.h"
#include "machine.h"
#include "keymap.h"
#include "yamledit.h"
#include "../host/host.h"

#include <yaml.h>

#include <ctype.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct reader {
    yaml_document_t *doc;
    char *error;
    size_t cap;
    size_t line;               /* the line being read, for messages */
    size_t model_line, options_line[RC_CONFIG_OPTIONS], media_line[RC_CONFIG_MEDIA];
    size_t key_line[RC_CONFIG_STICK_KEYS];                  /* the line of each of controllers.keys, 0: not set */
    size_t port_line[RC_CONFIG_PORTS];                      /* the line of each of ports.* */
} reader;

static int failf(reader *r, const char *format, ...) {
    if (!r->cap) return 0;
    size_t used = 0;
    if (r->line) used = (size_t)snprintf(r->error, r->cap, "line %zu: ", r->line);
    if (used < r->cap) {
        va_list args;
        va_start(args, format);
        vsnprintf(r->error + used, r->cap - used, format, args);
        va_end(args);
    }
    return 0;
}
static int fail(reader *r, const char *message) { return failf(r, "%s", message); }

static yaml_node_t *node(reader *r, int index) {
    yaml_node_t *n = r->doc ? yaml_document_get_node(r->doc, index) : NULL;
    if (n) r->line = n->start_mark.line + 1;
    return n;
}
static const char *scalar(reader *r, yaml_node_t *n) {
    if (n) r->line = n->start_mark.line + 1;
    if (!n || n->type != YAML_SCALAR_NODE || strlen((char *)n->data.scalar.value) != n->data.scalar.length) {
        fail(r, "Expected a single value");
        return NULL;
    }
    return (const char *)n->data.scalar.value;
}
static int mapping(reader *r, yaml_node_t *n) {
    if (n) r->line = n->start_mark.line + 1;
    if (!n || n->type != YAML_MAPPING_NODE) return fail(r, "Expected a mapping");
    for (yaml_node_pair_t *p = n->data.mapping.pairs.start; p < n->data.mapping.pairs.top; ++p) {
        const char *key = scalar(r, node(r, p->key));
        if (!key) return 0;
        const size_t line = r->line;
        for (yaml_node_pair_t *q = n->data.mapping.pairs.start; q < p; ++q) {
            const char *previous = scalar(r, node(r, q->key));
            if (!previous) return 0;
            if (strcmp(key, previous) == 0) {
                r->line = line;           /* the repeated key's line, not the first one's */
                return fail(r, "Duplicate configuration key");
            }
        }
    }
    return 1;
}
#define PAIRS(v, q) for (yaml_node_pair_t *q = (v)->data.mapping.pairs.start; q < (v)->data.mapping.pairs.top; ++q)
/* "key:" with nothing after it. */
static int empty(const yaml_node_t *v) { return v && v->type == YAML_SCALAR_NODE && !v->data.scalar.length; }

static int boolean(reader *r, yaml_node_t *n, int *out) {
    const char *s = scalar(r, n);
    if (s && strcmp(s, "true") == 0) { *out = 1; return 1; }
    if (s && strcmp(s, "false") == 0) { *out = 0; return 1; }
    return s ? fail(r, "Expected true or false") : 0;
}

static int integer(reader *r, yaml_node_t *n, const char *key, int lo, int hi, int *out) {
    const char *s = scalar(r, n);
    if (!s) return 0;
    char *end = NULL;
    const long v = strtol(s, &end, 10);
    if (!*s || !end || *end || v < lo || v > hi) return failf(r, "%s must be a whole number from %d to %d", key, lo, hi);
    *out = (int)v;
    return 1;
}

/* A short word (a model, an option's key or value, a slot's key). */
static int word(reader *r, const char *s, const char *what, char *out) {
    if (!s) return 0;
    if (!*s || strlen(s) >= RC_CONFIG_NAME) return failf(r, "%s must be a name of 1 to %d characters", what, RC_CONFIG_NAME - 1);
    snprintf(out, RC_CONFIG_NAME, "%s", s);
    return 1;
}

/* A path: relative ones are relative to the configuration's folder (base). */
static int path_value(reader *r, const char *s, const char *base, char *out) {
    const int absolute = s[0] == '/' || s[0] == '\\' || (s[0] && s[1] == ':');
    const int length = absolute ? snprintf(out, RC_CONFIG_PATH, "%s", s) : snprintf(out, RC_CONFIG_PATH, "%s%s", base, s);
    return length >= 0 && length < RC_CONFIG_PATH ? 1 : fail(r, "Path is too long");
}

/* --- the choice values ---------------------------------------------------------------- */

typedef struct named { const char *name; uint32_t value; } named;
static const named looks[] = {{"plain", RC_LOOK_PLAIN}, {"crt", RC_LOOK_CRT}, {NULL, 0}};
static const named aspects[] = {{"original", RC_CONFIG_ASPECT_ORIGINAL}, {"square", RC_CONFIG_ASPECT_SQUARE}, {NULL, 0}};
static const named scales[] = {{"fit", RC_CONFIG_SCALE_FIT}, {"integer", RC_CONFIG_SCALE_INTEGER}, {NULL, 0}};
static const named filters[] = {{"nearest", RC_CONFIG_FILTER_NEAREST}, {"linear", RC_CONFIG_FILTER_LINEAR},
                                {"sharp", RC_CONFIG_FILTER_SHARP}, {NULL, 0}};
static const named languages[] = {{"en", RC_LANG_EN}, {NULL, 0}};

static const char *name_of(const named *names, uint32_t value) {
    for (; names->name; ++names) if (names->value == value) return names->name;
    return "?";
}
const char *rc_config_look_name(uint32_t v) { return name_of(looks, v); }
const char *rc_config_aspect_name(uint32_t v) { return name_of(aspects, v); }
const char *rc_config_scale_name(uint32_t v) { return name_of(scales, v); }
const char *rc_config_filter_name(uint32_t v) { return name_of(filters, v); }
const char *rc_config_language_name(uint32_t v) { return name_of(languages, v); }

/* One of the names; the message lists them. */
static int choice(reader *r, yaml_node_t *n, const char *key, const named *names, uint32_t *out) {
    const char *s = scalar(r, n);
    if (!s) return 0;
    for (const named *p = names; p->name; ++p) if (!strcmp(s, p->name)) { *out = p->value; return 1; }
    char list[160];
    size_t used = 0;
    for (const named *p = names; p->name && used < sizeof list; ++p)
        used += (size_t)snprintf(list + used, sizeof list - used, "%s%s", p == names ? "" : p[1].name ? ", " : " or ", p->name);
    return failf(r, "%s must be %s", key, list);
}

/* --- the controllers ---------------------------------------------------------------------- */

static const char *const stick_sockets[RC_CONFIG_STICKS] = {"joystick1", "joystick2"};
static const char *const stick_key_names[RC_CONFIG_STICK_KEYS] = {"up", "down", "left", "right", "fire", "fire2"};
static const char *const stick_defaults[RC_CONFIG_STICK_KEYS] = {"Up", "Down", "Left", "Right", "Left Ctrl", "Left Shift"};
static int (*key_known)(const char *name);

const char *rc_config_stick_key_name(unsigned function) { return function < RC_CONFIG_STICK_KEYS ? stick_key_names[function] : "?"; }
const char *rc_config_stick_key_default(unsigned function) { return function < RC_CONFIG_STICK_KEYS ? stick_defaults[function] : ""; }
void rc_config_set_key_check(int (*known)(const char *name)) { key_known = known; }

const char *rc_config_stick_device_name(const rc_config_stick *stick, char *buffer, size_t capacity) {
    if(stick->device==RC_STICK_MOUSE)snprintf(buffer,capacity,"mouse");
    else if (stick->device == RC_STICK_KEYBOARD) snprintf(buffer, capacity, "keyboard");
    else if (stick->device == RC_STICK_CONTROLLER) snprintf(buffer, capacity, "controller %u", (unsigned)stick->pad);
    else snprintf(buffer, capacity, "none");
    return buffer;
}

/* Key names compare without regard to case, as the host's lookup does. */
static int same_key(const char *a, const char *b) {
    for (; *a && *b; ++a, ++b) if (tolower((unsigned char)*a) != tolower((unsigned char)*b)) return 0;
    return *a == *b;
}

/* A name no key has, or one the window keeps. */
static int key_name_problem(const char *name, const char **reserved) {
    static const struct { const char *name, *written; } window[] = {
        {"F9", "F9"}, {"F10", "F10"}, {"F11", "F11"}, {"F12", "F12"},
        {"Pause", "Pause"}, {"PrintScreen", "Print Screen"}, {"Print Screen", "Print Screen"}};
    const size_t n = strlen(name);
    if (!n || n >= RC_CONFIG_KEY_NAME) return RC_KEY_NAME_BAD;
    for (const char *p = name; *p; ++p) if ((unsigned char)*p < 0x20 || (unsigned char)*p > 0x7E) return RC_KEY_NAME_BAD;
    if (key_known && !key_known(name)) return RC_KEY_NAME_BAD;
    for (size_t i = 0; i < sizeof window / sizeof *window; ++i)
        if (same_key(name, window[i].name)) {
            if (reserved) *reserved = window[i].written;
            return RC_KEY_NAME_RESERVED;
        }
    return RC_KEY_NAME_OK;
}

int rc_config_stick_key_problem(const rc_config *c, unsigned function, const char *name, const char **reserved, unsigned *other) {
    const int problem = key_name_problem(name, reserved);
    if (problem != RC_KEY_NAME_OK) return problem;
    for (unsigned i = 0; i < RC_CONFIG_STICK_KEYS; ++i)
        if (i != function && same_key(c->stick_keys[i], name)) {
            if (other) *other = i;
            return RC_KEY_NAME_USED;
        }
    return RC_KEY_NAME_OK;
}

uint32_t rc_config_language_for_locale(const char *locale) { (void)locale; return RC_LANG_EN; }

uint32_t rc_config_default_language(void) { return RC_LANG_EN; }

void rc_config_defaults(rc_config *c) {
    memset(c, 0, sizeof *c);
    snprintf(c->model, sizeof c->model, "%s", RC_CONFIG_DEFAULT_MODEL);
    c->language = rc_config_default_language();
    c->look = RC_LOOK_CRT;
    c->aspect = RC_CONFIG_ASPECT_ORIGINAL;
    c->scale = RC_CONFIG_SCALE_FIT;
    c->filter = RC_CONFIG_FILTER_NEAREST;
    snprintf(c->palette,sizeof c->palette,"cjam");
    c->vsync = 1;
    c->sound = 1;
    c->volume = 100;
    c->sticks[0].device = RC_STICK_NONE;
    c->sticks[1].device = RC_STICK_CONTROLLER;c->sticks[1].pad=1;c->keyboard_port=1;
    for (unsigned i = 0; i < RC_CONFIG_STICK_KEYS; ++i) snprintf(c->stick_keys[i], sizeof c->stick_keys[i], "%s", stick_defaults[i]);
    c->state_slot = 1;
    snprintf(c->setup_page, sizeof c->setup_page, "machine");
}

/* --- the machine's options and media ---------------------------------------------------- */

const char *rc_config_option_value(const rc_config *c, const char *key) {
    for (unsigned i = 0; i < c->option_count; ++i) if (!strcmp(c->options[i].key, key)) return c->options[i].value;
    return NULL;
}

int rc_config_set_option(rc_config *c, const char *key, const char *value) {
    unsigned i = 0;
    while (i < c->option_count && strcmp(c->options[i].key, key)) ++i;
    if (i == RC_CONFIG_OPTIONS || strlen(key) >= RC_CONFIG_NAME || strlen(value) >= RC_CONFIG_NAME) return 0;
    if (i == c->option_count) ++c->option_count;
    snprintf(c->options[i].key, sizeof c->options[i].key, "%s", key);
    snprintf(c->options[i].value, sizeof c->options[i].value, "%s", value);
    return 1;
}

const rc_config_media *rc_config_media_find(const rc_config *c, const char *key) {
    for (unsigned i = 0; i < c->media_count; ++i) if (!strcmp(c->media[i].key, key)) return &c->media[i];
    return NULL;
}

rc_config_media *rc_config_media_slot(rc_config *c, const char *key) {
    for (unsigned i = 0; i < c->media_count; ++i) if (!strcmp(c->media[i].key, key)) return &c->media[i];
    if (c->media_count == RC_CONFIG_MEDIA || strlen(key) >= RC_CONFIG_NAME) return NULL;
    rc_config_media *m = &c->media[c->media_count++];
    memset(m, 0, sizeof *m);
    snprintf(m->key, sizeof m->key, "%s", key);
    m->fast = m->sound = m->autoload = m->full_speed = m->traps = m->save_traps = m->accelerate = 1;
    return m;
}

unsigned rc_config_tape_bits(const rc_config_media *m) {
    if (!m) return RC_TAPE_SOUND | RC_TAPE_TRAPS | RC_TAPE_SAVE_TRAPS | RC_TAPE_ACCELERATE;
    return (m->sound ? RC_TAPE_SOUND : 0u) | (m->fast && m->traps ? RC_TAPE_TRAPS : 0u) | (m->fast && m->save_traps ? RC_TAPE_SAVE_TRAPS : 0u) |
           (m->fast && m->accelerate ? RC_TAPE_ACCELERATE : 0u);
}

static void folder_of(const char *path, char *out);

/* --- the host ends of the byte ports ------------------------------------------------------ */

const rc_config_port *rc_config_port_find(const rc_config *c, const char *key) {
    for (unsigned i = 0; i < c->port_count; ++i) if (!strcmp(c->ports[i].key, key)) return &c->ports[i];
    return NULL;
}

rc_config_port *rc_config_port_slot(rc_config *c, const char *key) {
    for (unsigned i = 0; i < c->port_count; ++i) if (!strcmp(c->ports[i].key, key)) return &c->ports[i];
    if (c->port_count == RC_CONFIG_PORTS || strlen(key) >= RC_CONFIG_NAME) return NULL;
    rc_config_port *p = &c->ports[c->port_count++];
    memset(p, 0, sizeof *p);
    snprintf(p->key, sizeof p->key, "%s", key);
    return p;
}

/* A TCP target: HOST:PORT (a server: PORT or HOST:PORT), the port 1 to 65535, IPv4 only. */
int rc_config_port_problem(uint32_t type, const char *target) {
    if (type == RC_END_TCP || type == RC_END_TCP_LISTEN) {
        const char *colon = strrchr(target, ':');
        const char *digits = colon ? colon + 1 : target;
        if (type == RC_END_TCP && (!colon || colon == target)) return RC_PORT_NEEDS_HOST_PORT;
        char *end = NULL;
        const long port = *digits ? strtol(digits, &end, 10) : 0;
        if (!*digits || *end || port < 1 || port > 65535) return RC_PORT_BAD_TCP_PORT;
        if (colon && (colon == target || memchr(target, ':', (size_t)(colon - target)))) return type == RC_END_TCP ? RC_PORT_NEEDS_HOST_PORT : RC_PORT_LISTEN_FORM;
    } else if ((type == RC_END_SERIAL || type == RC_END_PIPE || type == RC_END_FILE) && !*target) return RC_PORT_EMPTY;
    return RC_PORT_OK;
}

const char *rc_config_port_problem_text(int problem) {
    static const char *const messages[] = {"", "A TCP connection needs HOST:PORT", "The TCP port must be a number from 1 to 65535",
                                           "A TCP server takes PORT or HOST:PORT", "The port's device, pipe or file is empty"};
    return problem >= 0 && problem < (int)(sizeof messages / sizeof *messages) ? messages[problem] : "";
}

/* "COM3", "com12" or a device path: a host serial port without its prefix. */
static int bare_serial(const char *s) {
    if (!strncmp(s, "/dev/", 5)) return 1;
    if ((s[0] | 0x20) == 'c' && (s[1] | 0x20) == 'o' && (s[2] | 0x20) == 'm' && s[3]) {
        for (const char *d = s + 3; *d; ++d) if (*d < '0' || *d > '9') return 0;
        return 1;
    }
    return 0;
}

#define PORT_WORDS "none, loopback, COMn, pipe:NAME, tcp:HOST:PORT, tcp-listen:PORT or file:FILE"

/* An end from its kind (serial, pipe, file, tcp, tcp-listen) and its value. */
static int port_set(reader *r, const char *key, const char *kind, const char *value, const char *base, rc_config_port *p) {
    uint32_t type;
    if (!strcmp(kind, "file")) type = RC_END_FILE;
    else if (!strcmp(kind, "serial")) type = RC_END_SERIAL;
    else if (!strcmp(kind, "pipe")) type = RC_END_PIPE;
    else if (!strcmp(kind, "tcp")) type = RC_END_TCP;
    else if (!strcmp(kind, "tcp-listen")) type = RC_END_TCP_LISTEN;
    else return failf(r, "ports.%s must be " PORT_WORDS, key);
    const int problem = rc_config_port_problem(type, value);
    if (problem) return failf(r, "ports.%s: %s", key, rc_config_port_problem_text(problem));
    p->type = type;
    if (type == RC_END_FILE) return path_value(r, value, base, p->path);
    if (strlen(value) >= sizeof p->path) return fail(r, "The port's name is too long");
    snprintf(p->path, sizeof p->path, "%s", value);
    return 1;
}

/* An end as the file writes it: "none", "loopback", "COM3", or "KIND:VALUE". */
static int port_value(reader *r, yaml_node_t *n, const char *key, const char *base, rc_config_port *p) {
    const char *s = scalar(r, n);
    if (!s) return 0;
    p->type = RC_END_NONE;
    p->path[0] = 0;
    if (!*s || !strcmp(s, "none")) return 1;
    if (!strcmp(s, "loopback")) { p->type = RC_END_LOOPBACK; return 1; }
    if (bare_serial(s)) return port_set(r, key, "serial", s, base, p);
    const char *colon = strchr(s, ':');
    char kind[16];
    if (!colon || (size_t)(colon - s) >= sizeof kind) return failf(r, "ports.%s must be " PORT_WORDS, key);
    memcpy(kind, s, (size_t)(colon - s));
    kind[colon - s] = 0;
    return port_set(r, key, kind, colon + 1, base, p);
}

const char *rc_config_port_text(const rc_config *c, const rc_config_port *p, char *buffer, size_t capacity) {
    char base[RC_CONFIG_PATH];
    folder_of(c->source, base);
    const size_t n = strlen(base);
    switch (p->type) {
    case RC_END_LOOPBACK: snprintf(buffer, capacity, "loopback"); break;
    case RC_END_SERIAL: snprintf(buffer, capacity, bare_serial(p->path) ? "%s" : "serial:%s", p->path); break;
    case RC_END_PIPE: snprintf(buffer, capacity, "pipe:%s", p->path); break;
    case RC_END_FILE: snprintf(buffer, capacity, "file:%s", n && !strncmp(p->path, base, n) ? p->path + n : p->path); break;
    case RC_END_TCP: snprintf(buffer, capacity, "tcp:%s", p->path); break;
    case RC_END_TCP_LISTEN: snprintf(buffer, capacity, "tcp-listen:%s", p->path); break;
    default: snprintf(buffer, capacity, "none"); break;
    }
    return buffer;
}

/* --- the sections ------------------------------------------------------------------- */

static int options_section(reader *r, yaml_node_t *v, rc_config *c) {
    if (empty(v)) return 1;
    if (!mapping(r, v)) return 0;
    PAIRS(v, q) {
        const char *name = scalar(r, node(r, q->key));
        const size_t line = r->line;
        if (c->option_count == RC_CONFIG_OPTIONS) return fail(r, "Too many machine options");
        rc_config_option *o = &c->options[c->option_count];
        if (!word(r, name, "An option", o->key) || !word(r, scalar(r, node(r, q->value)), "An option's value", o->value)) return 0;
        r->options_line[c->option_count++] = line;
    }
    return 1;
}

static int machine_section(reader *r, yaml_node_t *v, rc_config *c) {
    if (!mapping(r, v)) return 0;
    PAIRS(v, q) {
        const char *name = scalar(r, node(r, q->key));
        yaml_node_t *x = node(r, q->value);
        int ok;
        if (!strcmp(name, "model")) { ok = word(r, scalar(r, x), "machine.model", c->model); r->model_line = r->line; }
        else if (!strcmp(name, "options")) ok = options_section(r, x, c);
        else ok = fail(r, "Unknown machine key; expected model or options");
        if (!ok) return 0;
    }
    return 1;
}

static int media_section(reader *r, yaml_node_t *v, const char *base, rc_config *c) {
    if (empty(v)) return 1;
    if (!mapping(r, v)) return 0;
    PAIRS(v, q) {
        const char *name = scalar(r, node(r, q->key));
        const size_t line = r->line;
        yaml_node_t *slot = node(r, q->value);
        if (c->media_count == RC_CONFIG_MEDIA) return fail(r, "Too many media slots");
        if (strlen(name) >= RC_CONFIG_NAME || !*name) return fail(r, "A media slot's name is too long");
        rc_config_media *m = rc_config_media_slot(c, name);
        r->media_line[c->media_count - 1] = line;
        if (empty(slot)) continue;                              /* "tape:" with nothing: an empty slot */
        if (!mapping(r, slot)) return 0;
        PAIRS(slot, p) {
            const char *key = scalar(r, node(r, p->key));
            yaml_node_t *x = node(r, p->value);
            int ok;
            if (!strcmp(key, "image")) {
                const char *s = scalar(r, x);
                ok = s != NULL;
                if (ok && *s) ok = path_value(r, s, base, m->image);
            } else if (!strcmp(key, "write_protect")) ok = boolean(r, x, &m->write_protect);
            else if (!strcmp(key, "fast")) ok = boolean(r, x, &m->fast);
            else if (!strcmp(key, "sound")) ok = boolean(r, x, &m->sound);
            else if (!strcmp(key, "autoload")) ok = boolean(r, x, &m->autoload);
            else if (!strcmp(key, "full_speed")) ok = boolean(r, x, &m->full_speed);
            else if (!strcmp(key, "traps")) ok = boolean(r, x, &m->traps);
            else if (!strcmp(key, "save_traps")) ok = boolean(r, x, &m->save_traps);
            else if (!strcmp(key, "accelerate")) ok = boolean(r, x, &m->accelerate);
            else ok = fail(r, "Unknown media key; expected image, write_protect, fast, sound, autoload, full_speed, traps, save_traps or accelerate");
            if (!ok) return 0;
        }
    }
    return 1;
}

static int video_section(reader *r, yaml_node_t *v, rc_config *c) {
    if (!mapping(r, v)) return 0;
    PAIRS(v, q) {
        const char *name = scalar(r, node(r, q->key));
        yaml_node_t *x = node(r, q->value);
        int ok;
        if (!strcmp(name, "look")) ok = choice(r, x, "video.look", looks, &c->look);
        else if (!strcmp(name, "aspect")) ok = choice(r, x, "video.aspect", aspects, &c->aspect);
        else if (!strcmp(name, "scale")) ok = choice(r, x, "video.scale", scales, &c->scale);
        else if (!strcmp(name, "filter")) ok = choice(r, x, "video.filter", filters, &c->filter);
        else if (!strcmp(name, "border")) {static const named borders[]={{"full",RC_BORDER_FULL},{"minimal",RC_BORDER_MINIMAL},{"none",RC_BORDER_NONE},{NULL,0}};ok=choice(r,x,"video.border",borders,&c->border);}
        else if (!strcmp(name, "palette")) {const char *s=scalar(r,x);ok=s&&*s&&strlen(s)<sizeof c->palette&&!strpbrk(s,"/\\:\r\n");if(ok)snprintf(c->palette,sizeof c->palette,"%s",s);else fail(r,"video.palette needs a built-in id or a file name in palettes/");}
        else if (!strcmp(name, "fullscreen")) ok = boolean(r, x, &c->fullscreen);
        else if (!strcmp(name, "vsync")) ok = boolean(r, x, &c->vsync);
        else ok = fail(r, "Unknown video key; expected look, aspect, scale, filter, border, palette, fullscreen or vsync");
        if (!ok) return 0;
    }
    return 1;
}

static int sound_section(reader *r, yaml_node_t *v, rc_config *c) {
    if (!mapping(r, v)) return 0;
    PAIRS(v, q) {
        const char *name = scalar(r, node(r, q->key));
        yaml_node_t *x = node(r, q->value);
        int ok;
        if (!strcmp(name, "enabled")) ok = boolean(r, x, &c->sound);
        else if (!strcmp(name, "volume")) ok = integer(r, x, "sound.volume", 0, 100, &c->volume);
        else ok = fail(r, "Unknown sound key; expected enabled or volume");
        if (!ok) return 0;
    }
    return 1;
}

/* none, keyboard, or controller N. */
static int stick_device(reader *r, yaml_node_t *n, const char *key, rc_config_stick *st) {
    const char *s = scalar(r, n);
    if (!s) return 0;
    if(!strcmp(s,"mouse")){st->device=RC_STICK_MOUSE;st->pad=0;return 1;}
    if (!strcmp(s, "none")) { st->device = RC_STICK_NONE; st->pad = 0; return 1; }
    if (!strcmp(s, "keyboard")) { st->device = RC_STICK_KEYBOARD; st->pad = 0; return 1; }
    if (!strncmp(s, "controller ", 11) && s[11] >= '1' && s[11] <= '9' && !s[12]) {
        const unsigned pad = (unsigned)(s[11] - '0');
        if (pad <= RC_CONFIG_PADS) { st->device = RC_STICK_CONTROLLER; st->pad = pad; return 1; }
    }
    return failf(r, "%s must be none, keyboard or controller 1 to %d", key, RC_CONFIG_PADS);
}

static int stick_section(reader *r, yaml_node_t *v, unsigned index, rc_config *c) {
    if (empty(v)) return 1;
    if (!mapping(r, v)) return 0;
    PAIRS(v, q) {
        const char *name = scalar(r, node(r, q->key));
        yaml_node_t *x = node(r, q->value);
        char key[64];
        snprintf(key, sizeof key, "controllers.%s.%s", stick_sockets[index], name);
        if (!strcmp(name,"type")) {
            static const named types[]={{"joystick",RC_CONTROL_JOYSTICK},{"mouse1351",RC_CONTROL_MOUSE1351},{"paddles",RC_CONTROL_PADDLES},{"none",RC_CONTROL_NONE},{NULL,0}};
            if(!choice(r,x,key,types,&c->sticks[index].type))return 0;
        } else if(!strcmp(name,"device")){if(!stick_device(r,x,key,&c->sticks[index]))return 0;}
        else return fail(r,"Unknown joystick key; expected device or type");
    }
    return 1;
}

/* controllers.keys: the name of a host key for each function of the keyboard joystick. */
static int keys_section(reader *r, yaml_node_t *v, rc_config *c) {
    if (empty(v)) return 1;
    if (!mapping(r, v)) return 0;
    PAIRS(v, q) {
        const char *name = scalar(r, node(r, q->key));
        yaml_node_t *x = node(r, q->value);
        unsigned i = 0;
        while (i < RC_CONFIG_STICK_KEYS && strcmp(name, stick_key_names[i])) ++i;
        if (i == RC_CONFIG_STICK_KEYS) return fail(r, "Unknown key function; expected up, down, left, right, fire or fire2");
        const char *value = scalar(r, x), *reserved = NULL;
        if (!value) return 0;
        const int problem = key_name_problem(value, &reserved);
        if (problem == RC_KEY_NAME_BAD) return failf(r, "controllers.keys.%s must be the name of a host key, as SDL names it (Up, Left Ctrl, A, Space)", name);
        if (problem == RC_KEY_NAME_RESERVED) return failf(r, "controllers.keys.%s: %s belongs to the window; choose another key", name, reserved);
        snprintf(c->stick_keys[i], sizeof c->stick_keys[i], "%s", value);
        r->key_line[i] = r->line;
    }
    return 1;
}

static int keyboard_section(reader *r,yaml_node_t *v,rc_config *c){
    if(empty(v))return 1;if(!mapping(r,v))return 0;
    PAIRS(v,q){const char *key=scalar(r,node(r,q->key));const char *host=scalar(r,node(r,q->value));uint8_t keys[2];
        if(!key||!host)return 0;
        if(!rc_keymap_action(rc_family_of_model(RC_CONFIG_DEFAULT_MODEL,NULL),key,keys))return failf(r,"Unknown C64 key: %s",key);
        if(c->binding_count==RC_CONFIG_BINDINGS||strlen(key)>=32||key_name_problem(host,NULL)!=RC_KEY_NAME_OK)return fail(r,"Invalid or reserved keyboard binding");
        for(unsigned i=0;i<c->binding_count;++i)if(same_key(c->bindings[i].host,host)||same_key(c->bindings[i].key,key))return fail(r,"Duplicate keyboard binding");
        rc_config_binding *b=&c->bindings[c->binding_count++];snprintf(b->key,sizeof b->key,"%s",key);snprintf(b->host,sizeof b->host,"%s",host);
    }return 1;
}
static int controllers_section(reader *r, yaml_node_t *v, rc_config *c) {
    if (!mapping(r, v)) return 0;
    PAIRS(v, q) {
        const char *name = scalar(r, node(r, q->key));
        const size_t line = r->line;
        yaml_node_t *x = node(r, q->value);
        int ok = 0, found = 0;
        if (!strcmp(name, "keys")) { found = 1; ok = keys_section(r, x, c); }
        else if(!strcmp(name,"keyboard_port")){int port;found=1;ok=integer(r,x,"keyboard_port",1,2,&port);if(ok)c->keyboard_port=(unsigned)(port-1);}
        else if (!strcmp(name, "keyboard_joystick")) { found = 1; ok = boolean(r, x, &c->keyboard_joystick); }
        for (unsigned i = 0; i < RC_CONFIG_STICKS; ++i)
            if (!strcmp(name, stick_sockets[i])) { found = 1; ok = stick_section(r, x, i, c); }
        if (!found) { r->line = line; ok = fail(r, "Unknown controllers key; expected joystick1, joystick2, keys or keyboard_joystick"); }
        if (!ok) return 0;
    }
    return 1;
}

/* ports.KEY: the keys are the ones some family's port table has; whether this model has the port is checked once the model is
 * known (check_ports). */
static int ports_section(reader *r, yaml_node_t *v, const char *base, rc_config *c) {
    if (empty(v)) return 1;
    if (!mapping(r, v)) return 0;
    PAIRS(v, q) {
        const char *name = scalar(r, node(r, q->key));
        const size_t line = r->line;
        yaml_node_t *x = node(r, q->value);
        int known = 0;
        for (unsigned i = 0; i < rc_family_count() && !known; ++i)
            for (unsigned k = 0; k < rc_family_at(i)->port_count; ++k) if (!strcmp(rc_family_at(i)->ports[k].key, name)) known = 1;
        if (!known) {
            char list[160];
            size_t used = 0;
            for (unsigned i = 0; i < rc_family_count(); ++i)
                for (unsigned k = 0; k < rc_family_at(i)->port_count && used < sizeof list; ++k) {
                    const char *key = rc_family_at(i)->ports[k].key;
                    int seen = 0;
                    for (unsigned j = 0; j < i && !seen; ++j)
                        for (unsigned m = 0; m < rc_family_at(j)->port_count; ++m) if (!strcmp(rc_family_at(j)->ports[m].key, key)) seen = 1;
                    for (unsigned m = 0; m < k && !seen; ++m) if (!strcmp(rc_family_at(i)->ports[m].key, key)) seen = 1;
                    if (!seen) used += (size_t)snprintf(list + used, sizeof list - used, "%s%s", used ? ", " : "", key);
                }
            r->line = line;
            return failf(r, "Unknown port %s; the ports are %s", name, list);
        }
        rc_config_port *p = rc_config_port_slot(c, name);
        if (!p) return fail(r, "Too many ports");
        r->port_line[p - c->ports] = line;
        if (!port_value(r, x, name, base, p)) return 0;
    }
    return 1;
}

static int ui_section(reader *r, yaml_node_t *v, rc_config *c) {
    if (!mapping(r, v)) return 0;
    PAIRS(v, q) {
        const char *name = scalar(r, node(r, q->key));
        yaml_node_t *x = node(r, q->value);
        int ok;
        if (!strcmp(name, "language")) ok = choice(r, x, "ui.language", languages, &c->language);
        else if (!strcmp(name, "state_slot")) ok = integer(r, x, "ui.state_slot", 1, 10, &c->state_slot);
        else if (!strcmp(name, "setup_page")) {
            const char *word = scalar(r, x);
            size_t n = 0;
            if (!word) return 0;
            while (word[n] >= 'a' && word[n] <= 'z') ++n;
            ok = n && !word[n] && n < sizeof c->setup_page;
            if (ok) snprintf(c->setup_page, sizeof c->setup_page, "%s", word);
            else ok = fail(r, "ui.setup_page is the name of a page of the setup screen (media, machine...)");
        } else ok = fail(r, "Unknown ui key; expected language, state_slot or setup_page");
        if (!ok) return 0;
    }
    return 1;
}

/* The machine's part against the registry: the model exists, each option is
 * one of its family's and of this model with a value the option takes, each
 * media slot is one the model has. */
static int check_machine(reader *r, const rc_config *c) {
    unsigned model = 0;
    const rc_family *f = rc_family_of_model(c->model, &model);
    r->line = r->model_line;
    if (!f) {
        char list[512];
        size_t used = 0;
        for (unsigned i = 0; i < rc_family_count(); ++i)
            for (unsigned k = 0; k < rc_family_at(i)->model_count && used < sizeof list; ++k)
                used += (size_t)snprintf(list + used, sizeof list - used, "%s%s", used ? ", " : "", rc_family_at(i)->models[k].id);
        return failf(r, "machine.model: there is no model %s; the models are %s", c->model, list);
    }
    for (unsigned i = 0; i < c->option_count; ++i) {
        r->line = r->options_line[i];
        const int o = rc_family_option(f, c->options[i].key);
        if (o < 0 || !(f->options[o].models >> model & 1u)) return failf(r, "machine.options: %s has no option %s", f->models[model].name, c->options[i].key);
        if (rc_family_option_value(f, (unsigned)o, c->options[i].value) < 0) {
            char list[256];
            size_t used = 0;
            for (const char *const *w = f->options[o].values; *w && used < sizeof list; ++w)
                used += (size_t)snprintf(list + used, sizeof list - used, "%s%s", w == f->options[o].values ? "" : w[1] ? ", " : " or ", *w);
            return failf(r, "machine.options.%s must be %s", c->options[i].key, list);
        }
    }
    for (unsigned i = 0; i < c->media_count; ++i) {
        r->line = r->media_line[i];
        const int s = rc_family_media(f, c->media[i].key);
        if (s < 0 || !(f->media[s].models >> model & 1u)) return failf(r, "media: %s has no slot %s", f->models[model].name, c->media[i].key);
        if (f->media[s].kind != RC_MEDIA_TAPE && !(c->media[i].sound && c->media[i].autoload && c->media[i].full_speed && c->media[i].traps &&
                                                   c->media[i].save_traps && c->media[i].accelerate))
            return failf(r, "media.%s: sound, autoload, full_speed, traps, save_traps and accelerate are a tape's settings", c->media[i].key);
    }
    return 1;
}

/* Two functions of the keyboard joystick with the same key: each needs a key of its own. */
static int check_controllers(reader *r, const rc_config *c) {
    for (unsigned i = 0; i < RC_CONFIG_STICK_KEYS; ++i)
        for (unsigned j = i + 1; j < RC_CONFIG_STICK_KEYS; ++j)
            if (same_key(c->stick_keys[i], c->stick_keys[j])) {
                r->line = r->key_line[j] ? r->key_line[j] : r->key_line[i];
                return failf(r, "controllers.keys.%s and controllers.keys.%s are both %s: each needs a key of its own",
                             stick_key_names[i], stick_key_names[j], c->stick_keys[i]);
            }
    return 1;
}

/* The ends of the model's own ports against what the port is: a printer has no receive line to loop back. */
static int check_ports(reader *r, const rc_config *c) {
    unsigned model = 0;
    const rc_family *f = rc_family_of_model(c->model, &model);
    for (unsigned i = 0; f && i < c->port_count; ++i)
        for (unsigned k = 0; k < f->port_count; ++k)
            if (!strcmp(f->ports[k].key, c->ports[i].key) && (f->ports[k].models >> model & 1u) && f->ports[k].kind == RC_PORT_PARALLEL &&
                c->ports[i].type == RC_END_LOOPBACK) {
                r->line = r->port_line[i];
                return failf(r, "ports.%s: %s has no receive line to loop back; use none, COMn, pipe:NAME, tcp:HOST:PORT, tcp-listen:PORT or file:FILE",
                             c->ports[i].key, f->models[model].name);
            }
    return 1;
}

/* The folder part of a path, with its separator ("" for none). */
static void folder_of(const char *path, char *out) {
    snprintf(out, RC_CONFIG_PATH, "%s", path);
    char *slash = strrchr(out, '/'), *backslash = strrchr(out, '\\');
    if (backslash && (!slash || backslash > slash)) slash = backslash;
    if (slash) slash[1] = 0; else out[0] = 0;
}

int rc_config_parse(const char *text, size_t length, const char *source, rc_config *out, char *error, size_t capacity) {
    yaml_parser_t parser;
    yaml_document_t doc;
    static reader r;                    /* its line tables are large for a stack frame */
    memset(&r, 0, sizeof r);
    r.error = error;
    r.cap = capacity;
    if (capacity) error[0] = 0;
    if (strlen(source) >= RC_CONFIG_PATH) return fail(&r, "Configuration path is too long");
    if (!yaml_parser_initialize(&parser)) return fail(&r, "Cannot allocate the YAML parser");
    yaml_parser_set_input_string(&parser, (const unsigned char *)text, length);
    if (!yaml_parser_load(&parser, &doc)) {
        if (capacity) snprintf(error, capacity, "line %zu: %s", parser.problem_mark.line + 1, parser.problem ? parser.problem : "YAML syntax error");
        yaml_parser_delete(&parser);
        return 0;
    }
    r.doc = &doc;
    char base[RC_CONFIG_PATH];
    folder_of(source, base);
    static rc_config c;
    rc_config_defaults(&c);
    snprintf(c.source, sizeof c.source, "%s", source);
    yaml_node_t *root = yaml_document_get_root_node(&doc);
    int ok = 1;
    /* An empty file is all defaults. */
    if (root && !empty(root)) {
        ok = mapping(&r, root);
        if (ok) PAIRS(root, p) {
            const char *key = scalar(&r, node(&r, p->key));
            yaml_node_t *v = node(&r, p->value);
            if (!key) { ok = 0; break; }
            if (!strcmp(key, "machine")) ok = machine_section(&r, v, &c);
            else if (!strcmp(key, "media")) ok = media_section(&r, v, base, &c);
            else if (!strcmp(key, "video")) ok = video_section(&r, v, &c);
            else if (!strcmp(key, "sound")) ok = sound_section(&r, v, &c);
            else if (!strcmp(key,"keyboard")) ok=keyboard_section(&r,v,&c);
            else if (!strcmp(key, "controllers")) ok = controllers_section(&r, v, &c);
            else if (!strcmp(key, "ports")) ok = ports_section(&r, v, base, &c);
            else if (!strcmp(key, "ui")) ok = ui_section(&r, v, &c);
            else ok = fail(&r, "Unknown section; expected machine, media, video, sound, keyboard, controllers, ports or ui");
            if (!ok) break;
        }
    }
    yaml_document_delete(&doc);
    if (ok) ok = check_machine(&r, &c) && check_controllers(&r, &c) && check_ports(&r, &c);
    if (ok) {
        r.doc = NULL;
        r.line = 0;
        if (!yaml_parser_load(&parser, &doc)) ok = fail(&r, "Invalid trailing YAML document");
        else {
            if (yaml_document_get_root_node(&doc)) ok = fail(&r, "Only one YAML document is allowed");
            yaml_document_delete(&doc);
        }
    }
    yaml_parser_delete(&parser);
    if (ok) {
        if (capacity) error[0] = 0;
        *out = c;
    }
    return ok;
}

int rc_config_load(const char *path, rc_config *out, char *error, size_t capacity) {
    FILE *f = fopen(path, "rb");
    if (!f) {
        if (capacity) snprintf(error, capacity, "Cannot open the configuration");
        return 0;
    }
    char *text = NULL;
    long size = -1;
    if (!fseek(f, 0, SEEK_END)) size = ftell(f);
    int ok = size >= 0 && size < 1024 * 1024 && !fseek(f, 0, SEEK_SET) && (text = malloc((size_t)size + 1)) != NULL &&
             fread(text, 1, (size_t)size, f) == (size_t)size;
    fclose(f);
    if (!ok) {
        free(text);
        if (capacity) snprintf(error, capacity, "Cannot read the configuration");
        return 0;
    }
    text[size] = 0;
    ok = rc_config_parse(text, (size_t)size, path, out, error, capacity);
    free(text);
    return ok;
}

/* --- writing back ----------------------------------------------------------------- */

/* A key set to a scalar unless it already holds it (its quoting then
 * stays as the user wrote it). mode 1: a path (single quotes keep its
 * backslashes); mode 2: written as it is (true, false, numbers). */
static int set_scalar(struct rc_ydoc *doc, const char *const *path, size_t depth, const char *value, int mode) {
    char now[RC_CONFIG_PATH * 2 + 8], quoted[RC_CONFIG_PATH * 2 + 8];
    if (rc_ydoc_get(doc, path, depth, now, sizeof now)) {
        if (!strcmp(now, value)) return 1;
    } else if (!*value && rc_ydoc_has(doc, path, depth)) return 1;     /* "key:" with nothing: empty already */
    if (!*value) return rc_ydoc_set(doc, path, depth, "\"\"");
    if (mode == 2) return rc_ydoc_set(doc, path, depth, value);
    return rc_ydoc_set(doc, path, depth, mode == 1 ? rc_ydoc_quote_path(value, quoted, sizeof quoted) : rc_ydoc_quote(value, quoted, sizeof quoted));
}
static int set(struct rc_ydoc *doc, const char *section, const char *key, const char *value) {
    const char *path[2] = {section, key};
    return set_scalar(doc, path, 2, value, 0);
}
static int set_bool(struct rc_ydoc *doc, const char *section, const char *key, int value) {
    const char *path[2] = {section, key};
    return set_scalar(doc, path, 2, value ? "true" : "false", 2);
}
/* A setting that is written only when it is not its default or the file
 * names it already: a file is not filled with keys it never had. */
static int set_unless_default(struct rc_ydoc *doc, const char *const *path, size_t depth, const char *value, int mode, int is_default) {
    if (is_default && !rc_ydoc_has(doc, path, depth)) return 1;
    return set_scalar(doc, path, depth, value, mode);
}
/* A path as the file holds it: relative to the configuration's folder when
 * it lies inside it. */
static const char *stored_path(const rc_config *c, const char *path) {
    char base[RC_CONFIG_PATH];
    folder_of(c->source, base);
    const size_t n = strlen(base);
    return n && !strncmp(path, base, n) ? path + n : path;
}

int rc_config_store(const rc_config *c, struct rc_ydoc *doc) {
    int ok = set(doc, "machine", "model", c->model);
    /* The options the file has that the configuration no longer does (another model's) go. */
    static char keys[RC_CONFIG_OPTIONS * 2][128];
    const char *options[2] = {"machine", "options"};
    const size_t had = rc_ydoc_keys(doc, options, 2, keys, RC_CONFIG_OPTIONS * 2);
    for (size_t i = 0; ok && i < had; ++i)
        if (!rc_config_option_value(c, keys[i])) {
            const char *path[3] = {"machine", "options", keys[i]};
            ok = rc_ydoc_remove(doc, path, 3);
        }
    for (unsigned i = 0; ok && i < c->option_count; ++i) {
        const char *path[3] = {"machine", "options", c->options[i].key};
        ok = set_scalar(doc, path, 3, c->options[i].value, 0);
    }
    for (unsigned i = 0; ok && i < c->media_count; ++i) {
        const rc_config_media *m = &c->media[i];
        const char *image[3] = {"media", m->key, "image"}, *protect[3] = {"media", m->key, "write_protect"}, *fast[3] = {"media", m->key, "fast"};
        ok = set_unless_default(doc, image, 3, m->image[0] ? stored_path(c, m->image) : "", 1, !m->image[0]) &&
             set_unless_default(doc, protect, 3, m->write_protect ? "true" : "false", 2, !m->write_protect) &&
             set_unless_default(doc, fast, 3, m->fast ? "true" : "false", 2, m->fast);
        static const char *const NAMES[] = {"sound", "autoload", "full_speed", "traps", "save_traps", "accelerate"};
        const int values[] = {m->sound, m->autoload, m->full_speed, m->traps, m->save_traps, m->accelerate};
        for (size_t k = 0; ok && k < sizeof NAMES / sizeof *NAMES; ++k) {
            const char *path[3] = {"media", m->key, NAMES[k]};
            ok = set_unless_default(doc, path, 3, values[k] ? "true" : "false", 2, values[k]);
        }
    }
    ok = ok && set(doc, "video", "look", rc_config_look_name(c->look)) && set(doc, "video", "aspect", rc_config_aspect_name(c->aspect)) &&
         set(doc,"video","palette",c->palette) && set(doc,"video","border",c->border==RC_BORDER_NONE?"none":c->border==RC_BORDER_MINIMAL?"minimal":"full") &&
         set(doc, "video", "scale", rc_config_scale_name(c->scale)) && set(doc, "video", "filter", rc_config_filter_name(c->filter)) &&
         set_bool(doc, "video", "fullscreen", c->fullscreen) && set_bool(doc, "video", "vsync", c->vsync) &&
         set_bool(doc, "sound", "enabled", c->sound);
    if (ok) {
        const char *volume[2] = {"sound", "volume"};
        char number[16];
        snprintf(number, sizeof number, "%d", c->volume);
        ok = set_scalar(doc, volume, 2, number, 2);
    }
    for (unsigned i = 0; ok && i < RC_CONFIG_STICKS; ++i) {
        const char *path[3] = {"controllers", stick_sockets[i], "device"};
        char name[32];
        rc_config_stick_device_name(&c->sticks[i], name, sizeof name);
        ok = set_unless_default(doc, path, 3, name, 0, (i==0?c->sticks[i].device==RC_STICK_NONE:c->sticks[i].device==RC_STICK_CONTROLLER&&c->sticks[i].pad==1));
    }
    for(unsigned i=0;ok&&i<RC_CONFIG_STICKS;++i){
        const char *path[]={"controllers",stick_sockets[i],"type"};const char *types[]={"joystick","mouse1351","paddles","none"};
        ok=set_unless_default(doc,path,3,types[c->sticks[i].type],0,c->sticks[i].type==RC_CONTROL_JOYSTICK);
    }
    for(unsigned i=0;ok&&i<c->binding_count;++i){const char *path[]={"keyboard",c->bindings[i].key};ok=set_scalar(doc,path,2,c->bindings[i].host,1);}
    for (unsigned i = 0; ok && i < RC_CONFIG_STICK_KEYS; ++i) {
        const char *path[3] = {"controllers", "keys", stick_key_names[i]};
        ok = set_unless_default(doc, path, 3, c->stick_keys[i], 0, same_key(c->stick_keys[i], stick_defaults[i]));
    }
    for (unsigned i = 0; ok && i < c->port_count; ++i) {
        const rc_config_port *p = &c->ports[i];
        const char *path[2] = {"ports", p->key};
        char text[RC_CONFIG_PATH * 2 + 16];
        rc_config_port_text(c, p, text, sizeof text);
        /* "tcp:127.0.0.1:2323" and "file:out.txt" read well unquoted; anything odd is quoted. */
        int plain = isalnum((unsigned char)text[0]) || text[0] == '/';
        for (const char *t = text; *t && plain; ++t) plain = isalnum((unsigned char)*t) || strchr("._:/-", *t) != NULL;
        ok = set_unless_default(doc, path, 2, text, plain ? 2 : 1, p->type == RC_END_NONE);
    }
    if(ok){const char *path[]={"controllers","keyboard_port"};ok=set_unless_default(doc,path,2,c->keyboard_port?"2":"1",2,c->keyboard_port==1);}
    if (ok) {
        const char *path[2] = {"controllers", "keyboard_joystick"}, *autoload[2] = {"ui", "autoload"};
        ok = set_unless_default(doc, path, 2, c->keyboard_joystick ? "true" : "false", 2, !c->keyboard_joystick) &&
             (!rc_ydoc_has(doc, autoload, 2) || rc_ydoc_remove(doc, autoload, 2));      /* an older file's: the tape slot has it now */
    }
    if (ok) {
        const char *path[2] = {"ui", "state_slot"};
        char number[16];
        snprintf(number, sizeof number, "%d", c->state_slot);
        ok = set_unless_default(doc, path, 2, number, 2, c->state_slot == 1);
    }
    if (ok) {
        const char *path[2] = {"ui", "setup_page"};
        ok = set_unless_default(doc, path, 2, c->setup_page, 0, !strcmp(c->setup_page, "machine"));
    }
    return ok && set(doc, "ui", "language", rc_config_language_name(c->language));
}
