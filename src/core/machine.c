/* The machine interface (machine.h): the registry of families, the public
 * calls dispatched to the board that runs, and the frame as colours.
 *  */
#include "machine_impl.h"

#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* PLAN.md section 5: fixed-width types and 8-bit bytes. */
_Static_assert(CHAR_BIT == 8, "ReC64 needs 8-bit bytes");
_Static_assert(sizeof(uint16_t) == 2 && sizeof(uint32_t) == 4 && sizeof(uint64_t) == 8, "fixed-width types");

/* --- the registry ---------------------------------------------------------------------- */

/* Every family built in; a board's package adds its own here. */
static const rc_family *const FAMILIES[] = {&rc_c64_family,&rc_stub_family};
#define FAMILY_COUNT (sizeof FAMILIES / sizeof FAMILIES[0])
#define MAX_HASHES 8u

unsigned rc_family_count(void) { return (unsigned)FAMILY_COUNT; }
const rc_family *rc_family_at(unsigned index) { return index < FAMILY_COUNT ? FAMILIES[index] : NULL; }

const rc_family *rc_family_of_model(const char *id, unsigned *model) {
    for (size_t i = 0; i < FAMILY_COUNT; ++i)
        for (unsigned k = 0; k < FAMILIES[i]->model_count; ++k)
            if (!strcmp(FAMILIES[i]->models[k].id, id)) {
                if (model) *model = k;
                return FAMILIES[i];
            }
    return NULL;
}

int rc_family_option(const rc_family *f, const char *key) {
    for (unsigned i = 0; i < f->option_count; ++i) if (!strcmp(f->options[i].key, key)) return (int)i;
    return -1;
}
int rc_family_option_value(const rc_family *f, unsigned option, const char *word) {
    if (option >= f->option_count) return -1;
    for (int i = 0; f->options[option].values[i]; ++i) if (!strcmp(f->options[option].values[i], word)) return i;
    return -1;
}
int rc_family_media(const rc_family *f, const char *key) {
    for (unsigned i = 0; i < f->media_count; ++i) if (!strcmp(f->media[i].key, key)) return (int)i;
    return -1;
}
/* Key names compare without regard to case. */
static int same_name(const char *a, const char *b) {
    for (; *a && *b; ++a, ++b) {
        const int x = *a >= 'a' && *a <= 'z' ? *a - 32 : *a, y = *b >= 'a' && *b <= 'z' ? *b - 32 : *b;
        if (x != y) return 0;
    }
    return *a == *b;
}
int rc_family_key(const rc_family *f, const char *name) {
    if(!strcmp(f->id,"c64")&&same_name(name,"ENTER"))name="RETURN";
    for (unsigned i = 0; i < f->key_count; ++i) if (same_name(f->keys[i].name, name)) return (int)i;
    return -1;
}
int rc_family_typing(const rc_family *f, int ch, uint8_t *key, uint8_t *shift) {
    for (unsigned i = 0; i < f->typing_count; ++i)
        if (f->typing[i].ch == (uint32_t)ch) {
            *key = f->typing[i].key;
            *shift = f->typing[i].shift;
            return 1;
        }
    return 0;
}

/* --- what applies ---------------------------------------------------------------------- */

/* One clause "key=word|word" or "key!=word|word" of `n` characters. */
static int clause_holds(const rc_family *f, unsigned model, const unsigned *values, const char *clause, size_t n) {
    char key[RC_CONFIG_NAME];
    size_t k = 0;
    while (k < n && clause[k] != '=' && clause[k] != '!') ++k;
    if (k == n || k >= sizeof key) return 0;
    memcpy(key, clause, k);
    key[k] = 0;
    const int negate = clause[k] == '!';
    size_t at = k + (negate ? 2u : 1u);
    const int option = rc_family_option(f, key);
    const int is_model = !strcmp(key, "model") && model < f->model_count;
    if ((!is_model && (option < 0 || option >= RC_CONFIG_OPTIONS)) || at > n) return 0;
    const char *now = is_model ? f->models[model].id : f->options[option].values[values[option]];
    int found = 0;
    while (at <= n && !found) {
        size_t end = at;
        while (end < n && clause[end] != '|') ++end;
        found = strlen(now) == end - at && !memcmp(now, clause + at, end - at);
        at = end + 1;
    }
    return negate ? !found : found;
}

static int condition_holds(const rc_family *f, unsigned model, const unsigned *values, const char *when, size_t n) {
    for (size_t at = 0; at <= n;) {
        size_t end = at;
        while (end < n && when[end] != '&') ++end;
        if (end > at && !clause_holds(f, model, values, when + at, end - at)) return 0;
        at = end + 1;
    }
    return 1;
}

int rc_family_condition(const rc_family *f, unsigned model, const unsigned *values, const char *when) {
    return !when || condition_holds(f, model, values, when, strlen(when));
}

/* The word of the first "condition:word" rule (joined by ';') whose condition holds, into `word`; 0 when none does. */
static int rule_word(const rc_family *f, unsigned model, const unsigned *values, const char *rules, char *word, size_t capacity) {
    for (const char *rule = rules; rule && *rule;) {
        const char *end = strchr(rule, ';');
        const size_t n = end ? (size_t)(end - rule) : strlen(rule);
        size_t colon = 0;
        while (colon < n && rule[colon] != ':') ++colon;
        if (colon < n && condition_holds(f, model, values, rule, colon)) {
            const size_t w = n - colon - 1 < capacity ? n - colon - 1 : capacity - 1;
            memcpy(word, rule + colon + 1, w);
            word[w] = 0;
            return 1;
        }
        rule += n + (end ? 1u : 0u);
    }
    return 0;
}

void rc_family_resolve(const rc_family *f, unsigned model, const rc_config *config, unsigned *values, uint8_t *state) {
    uint8_t forced[RC_CONFIG_OPTIONS] = {0};
    const unsigned count = f->option_count < RC_CONFIG_OPTIONS ? f->option_count : RC_CONFIG_OPTIONS;
    for (unsigned i = 0; i < RC_CONFIG_OPTIONS; ++i) values[i] = 0;
    for (unsigned i = 0; i < count; ++i) {
        /* an option the model lacks is at its initial value, whatever a file written for another model says */
        const char *word = config && (f->options[i].models >> model & 1u) ? rc_config_option_value(config, f->options[i].key) : NULL;
        const int value = word ? rc_family_option_value(f, i, word) : -1;
        values[i] = value >= 0 ? (unsigned)value : f->options[i].initial;
    }
    /* A forced value can make another rule hold: a few rounds settle the short chains the tables have. */
    for (int round = 0; round < 3; ++round)
        for (unsigned i = 0; i < count; ++i) {
            const rc_option_desc *o = &f->options[i];
            if (!o->forced || !(o->models >> model & 1u)) continue;
            char word[RC_CONFIG_NAME];
            const int value = rule_word(f, model, values, o->forced, word, sizeof word) ? rc_family_option_value(f, i, word) : -1;
            if (value >= 0) { values[i] = (unsigned)value; forced[i] = 1; }
        }
    if (!state) return;
    for (unsigned i = 0; i < RC_CONFIG_OPTIONS; ++i) {
        state[i] = 0;
        if (i >= count || !(f->options[i].models >> model & 1u) || !rc_family_condition(f, model, values, f->options[i].when)) continue;
        state[i] = (uint8_t)(RC_OPTION_PRESENT | (forced[i] ? RC_OPTION_FORCED : 0));
    }
}

int rc_family_has_media(const rc_family *f, unsigned model, const unsigned *values, unsigned slot) {
    return slot < f->media_count && (f->media[slot].models >> model & 1u) && rc_family_condition(f, model, values, f->media[slot].when);
}
int rc_family_has_port(const rc_family *f, unsigned model, const unsigned *values, unsigned port) {
    return port < f->port_count && (f->ports[port].models >> model & 1u) && rc_family_condition(f, model, values, f->ports[port].when);
}

/* --- life cycle ------------------------------------------------------------------------ */

struct rc_machine {
    const rc_machine_impl *impl;
    void *self;
    const rc_family *family;
    unsigned model;
    unsigned options[RC_CONFIG_OPTIONS];        /* the value of each of the family's options the machine was built with */
};

int rc_machine_host_ok(void) {
    const uint16_t one = 1;
    uint8_t first;
    memcpy(&first, &one, 1);
    return first == 1;
}

rc_machine *rc_machine_create(const rc_config *config, const rc_rom_index *roms, char *error, size_t capacity) {
    if (!rc_machine_host_ok()) {
        if (capacity) snprintf(error, capacity, "This host is not little-endian: ReC64 cannot run on it");
        return NULL;
    }
    rc_machine_setup setup;
    memset(&setup, 0, sizeof setup);
    setup.family = rc_family_of_model(config->model, &setup.model);
    if (!setup.family) {
        if (capacity) snprintf(error, capacity, "There is no model %s", config->model);
        return NULL;
    }
    setup.roms = roms;
    setup.wanted_family = setup.family;
    setup.wanted_model = setup.model;
    /* Without one of its ROMs the model cannot be built: the test machine's screen says which. */
    for (const char *const *key = setup.family->models[setup.model].roms; key && *key; ++key)
        if (!roms || !rc_rom_index_find(roms, rc_romdb_find(*key))) {
            setup.family = &rc_stub_family;
            setup.model = 0;
            break;
        }
    unsigned options[RC_CONFIG_OPTIONS] = {0};
    rc_family_resolve(setup.family, setup.model, setup.family == setup.wanted_family ? config : NULL, options, NULL);
    setup.options = options;
    void *self = setup.family->create(&setup, error, capacity);
    if (!self) return NULL;
    rc_machine *m = malloc(sizeof *m);
    if (!m) {
        setup.family->impl->destroy(self);
        if (capacity) snprintf(error, capacity, "Out of memory");
        return NULL;
    }
    m->impl = setup.family->impl;
    m->self = self;
    m->family = setup.family;
    m->model = setup.model;
    memcpy(m->options, options, sizeof m->options);
    return m;
}

void rc_machine_destroy(rc_machine *m) {
    if (!m) return;
    m->impl->destroy(m->self);
    free(m);
}

const rc_family *rc_machine_family(const rc_machine *m) { return m->family; }
unsigned rc_machine_model(const rc_machine *m) { return m->model; }
const unsigned *rc_machine_options(const rc_machine *m) { return m->options; }
int rc_machine_has_media(const rc_machine *m, unsigned slot) { return rc_family_has_media(m->family, m->model, m->options, slot); }
int rc_machine_has_port(const rc_machine *m, unsigned port) { return rc_family_has_port(m->family, m->model, m->options, port); }
const char *rc_machine_description(const rc_machine *m) { return m->impl->description(m->self); }
const char *rc_machine_error(const rc_machine *m) { return m->impl->error(m->self); }
void rc_machine_reset(rc_machine *m, int mode) { m->impl->reset(m->self, mode); }
void rc_machine_nmi(rc_machine *m) { if (m->impl->nmi) m->impl->nmi(m->self); }
uint32_t rc_machine_clock(const rc_machine *m) { return m->impl->clock(m->self); }
rc_run_status rc_machine_run(rc_machine *m, uint64_t ticks) { return m->impl->run(m->self, ticks); }
uint64_t rc_machine_time(const rc_machine *m) { return m->impl->time(m->self); }
uint64_t rc_machine_frame_period(const rc_machine *m) { return m->impl->frame_period(m->self); }
uint64_t rc_machine_frames(const rc_machine *m) {
    if (m->impl->frames) return m->impl->frames(m->self);
    const uint64_t period = m->impl->frame_period(m->self);
    return period ? m->impl->time(m->self) / period : 0;
}
int rc_machine_frame(rc_machine *m, rc_frame *frame) { return m->impl->frame(m->self, frame); }
void rc_machine_border(rc_machine *m,unsigned border){if(m->impl->border)m->impl->border(m->self,border);}
size_t rc_machine_text(rc_machine *m, char *out, size_t capacity) {
    if (capacity) out[0] = 0;
    return m->impl->text ? m->impl->text(m->self, out, capacity) : 0;
}
uint32_t rc_machine_audio_rate(const rc_machine *m) { return m->impl->audio_rate(m->self); }
size_t rc_machine_audio_pull(rc_machine *m, int16_t *stereo, size_t frames) { return m->impl->audio_pull(m->self, stereo, frames); }
void rc_machine_key(rc_machine *m, unsigned key, int down) { if (key < m->family->key_count) m->impl->key(m->self, key, down); }
void rc_machine_joystick(rc_machine *m, unsigned stick, unsigned held) { if (m->impl->joystick) m->impl->joystick(m->self, stick, held); }

int rc_machine_insert(rc_machine *m, unsigned slot, rc_media *media, char *error, size_t capacity) {
    if (!m->impl->insert || !rc_machine_has_media(m, slot)) {
        if (capacity) snprintf(error, capacity, "There is no such slot");
        return 0;
    }
    return m->impl->insert(m->self, slot, media, error, capacity);
}
rc_media *rc_machine_media(const rc_machine *m, unsigned slot) { return slot < m->family->media_count ? m->impl->media(m->self, slot) : NULL; }
int rc_machine_eject(rc_machine *m, unsigned slot) { return slot < m->family->media_count && m->impl->eject(m->self, slot); }
unsigned rc_machine_activity(const rc_machine *m) { return m->impl->activity(m->self); }
int rc_machine_control(rc_machine *m, unsigned slot, unsigned command, uint32_t value) {
    return m->impl->control && slot < m->family->media_count ? m->impl->control(m->self, slot, command, value) : 0;
}

unsigned rc_machine_tape_options(rc_machine *m, unsigned slot, unsigned bits) {
    return m->impl->tape_options ? m->impl->tape_options(m->self, slot, bits) : 0u;
}
const char *rc_machine_autoload(const rc_machine *m, unsigned slot, int *power_cycle) {
    int cycle = 1;
    const char *text = m->impl->autoload ? m->impl->autoload(m->self, slot, &cycle) : m->family->models[m->model].autoload;
    if (power_cycle) *power_cycle = cycle;
    return text;
}

struct rc_tape_deck *rc_machine_tape_deck(rc_machine *m, unsigned slot) {
    return m->impl->tape_deck && slot < m->family->media_count ? m->impl->tape_deck(m->self, slot) : NULL;
}

void rc_machine_set_ports(rc_machine *m, const rc_port_host *host) { if (m->impl->set_ports) m->impl->set_ports(m->self, host); }
size_t rc_machine_port_receive(rc_machine *m, unsigned port, const uint8_t *data, size_t count) {
    return m->impl->port_receive ? m->impl->port_receive(m->self, port, data, count) : count;
}
void rc_machine_port_modem(rc_machine *m, unsigned port, int dcd, int cts, int dsr, int ri) {
    if (m->impl->port_modem) m->impl->port_modem(m->self, port, dcd, cts, dsr, ri);
}

/* --- states ---------------------------------------------------------------------------- */

void rc_machine_save(const rc_machine *m, rc_state_writer *w) {
    uint8_t hashes[MAX_HASHES * 32u];
    const uint32_t count = m->impl->rom_hashes(m->self, hashes, MAX_HASHES);
    rc_sw_header(w, m->family->id, m->family->models[m->model].id, hashes, count);
    /* The options that shape the machine: a state fits only a machine built with the same ones. */
    rc_sw_begin(w, "OPTS", 0);
    for (unsigned i = 0; i < m->family->option_count && i < RC_CONFIG_OPTIONS; ++i) {
        const rc_option_desc *o = &m->family->options[i];
        if (!(o->flags & RC_OPTION_RESET) || !(o->models >> m->model & 1u)) continue;
        rc_sw_str(w, o->key);
        rc_sw_str(w, o->values[m->options[i]]);
    }
    rc_sw_str(w, "");
    rc_sw_end(w);
    m->impl->save(m->self, w);
}

int rc_machine_load(rc_machine *m, rc_state_reader *r, char *error, size_t capacity) {
    uint8_t hashes[MAX_HASHES * 32u];
    const uint32_t count = m->impl->rom_hashes(m->self, hashes, MAX_HASHES);
    if (strcmp(r->family, m->family->id) || strcmp(r->model, m->family->models[m->model].id)) {
        if (capacity) snprintf(error, capacity, "The state is of another machine (%s)", r->model);
        return 0;
    }
    if (r->hash_count != count || (count && memcmp(r->hashes, hashes, (size_t)count * 32u))) {
        if (capacity) snprintf(error, capacity, "The state was saved with other ROMs");
        return 0;
    }
    if (rc_sr_chunk(r, "OPTS", 0))
        for (;;) {
            char key[RC_CONFIG_NAME], word[RC_CONFIG_NAME];
            rc_sr_str(r, key, sizeof key);
            if (!key[0] || !rc_sr_ok(r)) break;
            rc_sr_str(r, word, sizeof word);
            /* Older C64 snapshots named the chip by the machine's default. */
            if(!strcmp(m->family->id,"c64")&&!strcmp(key,"sid")&&!strcmp(word,"default"))
                snprintf(word,sizeof word,"%s",m->model&1u?"8580":"6581");
            const int o = rc_family_option(m->family, key);
            if (o >= 0 && o < RC_CONFIG_OPTIONS && strcmp(m->family->options[o].values[m->options[o]], word)) {
                if (capacity) snprintf(error, capacity, "The state was saved with %s: %s", key, word);
                return 0;
            }
        }
    if (!m->impl->load(m->self, r) || !rc_sr_ok(r)) {
        m->impl->reset(m->self, 1);
        if (capacity) snprintf(error, capacity, "The state is damaged");
        return 0;
    }
    return 1;
}

/* --- checks ---------------------------------------------------------------------------- */

void rc_machine_set_trace(rc_machine *m, rc_trace_fn fn, void *ctx) { if (m->impl->set_trace) m->impl->set_trace(m->self, fn, ctx); }
int rc_machine_peek(rc_machine *m, uint32_t address, uint8_t *value) {
    *value = 0xFF;
    return m->impl->peek && m->impl->peek(m->self, address, value);
}
int rc_machine_poke(rc_machine *m, uint32_t address, uint8_t value) { return m->impl->poke && m->impl->poke(m->self, address, value); }

/* --- the picture ----------------------------------------------------------------------- */

void rc_frame_argb(const rc_frame *f, uint32_t *out) {
    uint32_t colors[256];
    for (unsigned i = 0; i < 256; ++i) colors[i] = 0xFF000000u | (i < f->colors ? f->palette[i] : 0u);
    for (uint32_t y = 0; y < f->height; ++y) {
        const uint8_t *row = f->pixels + (size_t)y * f->pitch;
        uint32_t *o = out + (size_t)y * f->width;
        for (uint32_t x = 0; x < f->width; ++x) o[x] = colors[row[x]];
    }
}

void rc_machine_pointer(rc_machine *m,unsigned port,unsigned type,int x,int y,unsigned buttons){if(m->impl->pointer)m->impl->pointer(m->self,port,type,x,y,buttons);}
int rc_machine_get_status(const rc_machine *m,rc_machine_status *status){memset(status,0,sizeof *status);return m->impl->status?m->impl->status(m->self,status):0;}
