/* Portable machine interface. Boards describe models, options, media and
 * keyboards in tables, and implement machine_impl.h. The registry currently
 * contains the C64 family and a firmware-free diagnostic board. Guest time is CPU cycles of the
 * selected PAL or NTSC clock; the host handles pacing and presentation. */
#ifndef RC_MACHINE_H
#define RC_MACHINE_H

#include "../hw/hw.h"
#include "config.h"
#include "media.h"
#include "romdb.h"
#include "state.h"

#include <stddef.h>
#include <stdint.h>

typedef struct rc_machine rc_machine;

/* --- what a machine says of itself ---------------------------------------------------- */

#define RC_MODELS_ALL 0xFFFFFFFFu       /* a `models` mask: every model of the family */
#define RC_KEY_NONE 0xFFu

/* A model: its id is what machine.model names ("c64-pal"), unique among all
 * families. `roms` are the keys (romdb.h) of the ROMs it cannot run without,
 * NULL-terminated. `autoload` is what is typed to load from tape ("LOAD
 * \"\"\n" in the notation of the harness's `type`; NULL: nothing). */
typedef struct rc_model_desc {
    const char *id;
    const char *name;                   /* "C64" */
    const char *const *roms;
    const char *autoload;
} rc_model_desc;

/* An option of the machine (machine.options.KEY in rec64.yaml): one of a
 * list of values, written in the file as the words of `values` (NULL-
 * terminated; a switch is {"off", "on"}). `label` and `value_labels` are
 * text ids (text.h) for the setup screen; value_labels NULL shows the words.
 * `models` is a bit for each model of the family that has the option.
 *
 * `when` (NULL: always) is the condition under which the model has the option
 * at all: clauses joined by '&', each "key=word|word" (the option `key` of the
 * same family has one of those values) or "key!=word|word" (it has none of
 * them): "interface=divide|divmmc", "interface1=on&startup=cpm". What does not
 * apply is not shown by the setup screen. Media slots and ports have the same.
 * The key "model" stands for the model's id ("model=cobra80").
 * `forced` (NULL: never) gives the value the option must have under a
 * condition, as "condition:word" rules joined by ';' ("startup=cpm:on"): the
 * machine is built with that value whatever the file says, and the setup
 * screen shows it and does not let it be changed (rc_family_resolve). */
enum {
    RC_OPTION_RESET = 1,                /* a change builds the machine again */
    RC_OPTION_NOT_ORIGINAL = 2,         /* the setup screen marks it so */
    RC_OPTION_PORT = 4                  /* a setting of a byte port (its speed, its wiring): shown on the Ports page */
};
typedef struct rc_option_desc {
    const char *key;
    int label;
    const char *const *values;
    const int *value_labels;
    unsigned initial;                   /* the value a new configuration has */
    uint32_t models;
    unsigned flags;
    int hint;                           /* optional: the text id of a hint of its own for the setup screen, plus 1 (0: the general one) */
    const char *when;                   /* optional: the condition under which the model has the option */
    const char *forced;                 /* optional: "condition:word;..." the values it must have */
} rc_option_desc;

/* A media slot (media.KEY in rec64.yaml): `extensions` are the file name
 * endings it takes, lower case, separated by commas ("d64,t64,tap"). */
enum { RC_MEDIA_TAPE, RC_MEDIA_FLOPPY, RC_MEDIA_CART, RC_MEDIA_PROGRAM, RC_MEDIA_SID, RC_MEDIA_KINDS };
typedef struct rc_media_desc {
    const char *key;                    /* "tape", "a", "b", "card" */
    int label;
    unsigned kind;                      /* RC_MEDIA_* */
    const char *extensions;
    uint32_t models;
    const char *when;                   /* optional: the condition (as an option's) under which the model has the slot */
} rc_media_desc;

/* A byte port of the guest (ports.KEY in rec64.yaml). */
enum { RC_PORT_SERIAL, RC_PORT_PARALLEL };
typedef struct rc_port_desc {
    const char *key;                    /* "serial", "printer" */
    int label;
    unsigned kind;                      /* RC_PORT_* */
    uint32_t models;
    const char *when;                   /* optional: the condition under which the model has the port */
} rc_port_desc;

/* A key of the machine's keyboard; its index in the table is what
 * rc_machine_key takes. `name` is how the harness and the file call it
 * ("A", "ENTER", "CAPS SHIFT"); `host` the host key in the same place (SDL's
 * scancode name, NULL: none); `legend` the keycap's main marking; x, y, w
 * and h place the key on the on-screen keyboard in quarters of a key. */
typedef struct rc_key_desc {
    const char *name;
    const char *host;
    const char *legend;
    uint8_t x, y, w, h;
} rc_key_desc;

/* How a character is typed: a key, alone or with a shift key held. */
typedef struct rc_typing_desc {
    uint32_t ch;
    uint8_t key, shift;                 /* shift RC_KEY_NONE: alone */
} rc_typing_desc;

/* A host key the machine's keyboard has no key for, given as two of its
 * keys together (a shifted function key, for example). A host that
 * starts with "Shift+" is the key with the host's Shift held (Shift+' is the
 * double quote: SYMBOL SHIFT and P); the machine's own key for the host's
 * Shift is then let go while the combination is down. */
typedef struct rc_combo_desc {
    const char *host;
    uint8_t key, shift;
} rc_combo_desc;

struct rc_machine_impl;
struct rc_machine_setup;

typedef struct rc_family {
    const char *id;                     /* "hc" */
    const char *name;                   /* "C64" */
    const rc_model_desc *models;    unsigned model_count;
    const rc_option_desc *options;  unsigned option_count;
    const rc_media_desc *media;     unsigned media_count;
    const rc_port_desc *ports;      unsigned port_count;
    const rc_key_desc *keys;        unsigned key_count;
    const rc_typing_desc *typing;   unsigned typing_count;
    const rc_combo_desc *combos;    unsigned combo_count;
    unsigned joysticks;                 /* joystick sockets the family's machines can have */
    const struct rc_machine_impl *impl;
    void *(*create)(const struct rc_machine_setup *setup, char *error, size_t capacity);
} rc_family;

/* The registry. */
unsigned rc_family_count(void);
const rc_family *rc_family_at(unsigned index);
/* The family that has model `id`, and which of its models it is; NULL when
 * there is no such model. */
const rc_family *rc_family_of_model(const char *id, unsigned *model);
/* The index of a named thing in a family's tables; -1 when it has none. */
int rc_family_option(const rc_family *f, const char *key);
int rc_family_option_value(const rc_family *f, unsigned option, const char *word);
int rc_family_media(const rc_family *f, const char *key);
int rc_family_key(const rc_family *f, const char *name);
/* The keys that type a character; 0 when the keyboard cannot. */
int rc_family_typing(const rc_family *f, int ch, uint8_t *key, uint8_t *shift);

/* The value of every option of the family (an index into its values) for a model under a configuration: the forced value where
 * a rule holds, else the file's, else the initial one. `state` (optional) says of each option whether the model has it now
 * (RC_OPTION_PRESENT: its model bit and its `when`) and whether its value is forced (RC_OPTION_FORCED). Both arrays have
 * RC_CONFIG_OPTIONS entries. `config` NULL: the initial values. */
enum { RC_OPTION_PRESENT = 1, RC_OPTION_FORCED = 2 };
void rc_family_resolve(const rc_family *f, unsigned model, const rc_config *config, unsigned *values, uint8_t *state);
/* Whether a condition (an option's `when`) holds for those values; NULL holds. */
int rc_family_condition(const rc_family *f, unsigned model, const unsigned *values, const char *when);
/* Whether the model has a media slot or a port under those values. */
int rc_family_has_media(const rc_family *f, unsigned model, const unsigned *values, unsigned slot);
int rc_family_has_port(const rc_family *f, unsigned model, const unsigned *values, unsigned port);

/* --- life cycle ----------------------------------------------------------------- */

typedef enum rc_run_status {
    RC_RUN_OK,          /* ran the time asked */
    RC_RUN_ERROR        /* stopped: rc_machine_error says why */
} rc_run_status;

/* A machine for the configuration (machine.*), with its ROMs taken from the
 * index (copied: the index may go afterwards). NULL with the reason. Media
 * are inserted afterwards with rc_machine_insert. */
rc_machine *rc_machine_create(const rc_config *config, const rc_rom_index *roms, char *error, size_t capacity);
void rc_machine_destroy(rc_machine *m);
/* The family and the model that run (the test machine's when ROMs are missing). */
const rc_family *rc_machine_family(const rc_machine *m);
unsigned rc_machine_model(const rc_machine *m);
/* The value of each of the family's options the machine was built with (RC_CONFIG_OPTIONS entries). */
const unsigned *rc_machine_options(const rc_machine *m);
/* Whether the machine as built has the media slot, the port. */
int rc_machine_has_media(const rc_machine *m, unsigned slot);
int rc_machine_has_port(const rc_machine *m, unsigned port);
/* What runs, in words ("C64, Interface 1"). */
const char *rc_machine_description(const rc_machine *m);
const char *rc_machine_error(const rc_machine *m);
/* Power cycle (power_on 1: the state at switching on, memory cleared) or
 * the board's RESET line (power_on 0: memory kept). */
void rc_machine_reset(rc_machine *m, int power_on);
/* The NMI button, where the board has one (else nothing). */
void rc_machine_nmi(rc_machine *m);

/* --- time ----------------------------------------------------------------------- */

/* The CPU clock's rate in Hz: what a tick of guest time is. */
uint32_t rc_machine_clock(const rc_machine *m);
/* Runs until guest time has advanced by at least `ticks` (cycle boundaries are exact). */
rc_run_status rc_machine_run(rc_machine *m, uint64_t ticks);
uint64_t rc_machine_time(const rc_machine *m);
/* The video frame's period in ticks: the shell presents one frame per period. */
uint64_t rc_machine_frame_period(const rc_machine *m);
/* The frames the video has finished so far: the `serial` rc_machine_frame would give, at no cost. */
uint64_t rc_machine_frames(const rc_machine *m);

/* --- the picture ---------------------------------------------------------------- */

/* A frame as the machine's video produced it, border included: one byte per
 * pixel, rows `pitch` bytes apart, each an index into `palette` (`colors`
 * entries of 0x00RRGGBB). `aspect_w:aspect_h` is the shape the whole frame
 * had on the monitor. `serial` changes whenever the pixels change. */
typedef struct rc_frame {
    uint32_t width, height, pitch;
    const uint8_t *pixels;
    const uint32_t *palette;
    uint32_t colors;
    uint32_t aspect_w, aspect_h;
    uint64_t serial;
} rc_frame;
/* 0 when the machine shows nothing (the frame is then black). */
int rc_machine_frame(rc_machine *m, rc_frame *frame);
enum {RC_BORDER_FULL,RC_BORDER_MINIMAL,RC_BORDER_NONE};
void rc_machine_border(rc_machine *m,unsigned border);
/* The frame as ARGB8888 into out (width x height). */
void rc_frame_argb(const rc_frame *frame, uint32_t *out);
/* The characters on the screen as text, a line of the screen a line of the
 * text (the machine reads its own video memory back through its font):
 * what the harness's `text` and `expect` see. Returns the length; 0 when
 * the machine cannot say. */
size_t rc_machine_text(rc_machine *m, char *out, size_t capacity);

/* --- sound ---------------------------------------------------------------------- */

/* Interleaved stereo 16-bit frames at rc_machine_audio_rate (Hz; 0 when the
 * machine makes no sound): the frames the guest produced so far, at most
 * `frames`; returns how many were written. */
uint32_t rc_machine_audio_rate(const rc_machine *m);
size_t rc_machine_audio_pull(rc_machine *m, int16_t *stereo, size_t frames);

/* --- input ---------------------------------------------------------------------- */

/* A key of the family's key table pressed (down 1) or released. */
void rc_machine_key(rc_machine *m, unsigned key, int down);
/* Joystick socket `stick`: the directions and buttons held, RC_JOY_* bits.
 * What kind of joystick is in the socket is an option of the machine. */
enum { RC_JOY_UP = 1, RC_JOY_DOWN = 2, RC_JOY_LEFT = 4, RC_JOY_RIGHT = 8, RC_JOY_FIRE = 16, RC_JOY_FIRE2 = 32 };
void rc_machine_joystick(rc_machine *m, unsigned stick, unsigned held);
/* Relative mouse movement or absolute paddle ADC values, buttons bit 0/1. */
void rc_machine_pointer(rc_machine *m,unsigned port,unsigned type,int x,int y,unsigned buttons);

/* --- media ---------------------------------------------------------------------- */

/* A file in a slot of the family's media table (media.h). The file stays
 * the caller's: the machine uses it until it is ejected or the machine is
 * destroyed, and marks it changed when the guest writes to it. 0 with the
 * reason when the slot does not take it. */
int rc_machine_insert(rc_machine *m, unsigned slot, rc_media *media, char *error, size_t capacity);
/* The file in a slot, NULL when empty. */
rc_media *rc_machine_media(const rc_machine *m, unsigned slot);
/* Takes the file out (the caller may close it then); 0 when empty. */
int rc_machine_eject(rc_machine *m, unsigned slot);
/* Bit n set while slot n works (a drive's motor turns, the tape runs). */
unsigned rc_machine_activity(const rc_machine *m);
/* The transport of a slot: the tape recorder's keys. `value` is the block
 * for RC_MEDIA_SEEK. Returns 0 when the slot has no such control. */
enum { RC_MEDIA_PLAY, RC_MEDIA_STOP, RC_MEDIA_REWIND, RC_MEDIA_NEXT, RC_MEDIA_PREVIOUS, RC_MEDIA_SEEK, RC_MEDIA_RECORD,
       RC_MEDIA_PROTECT, RC_MEDIA_FLUSH, RC_MEDIA_FREEZE, RC_MEDIA_SONG, RC_MEDIA_PAUSE };
/* RC_MEDIA_FLUSH transfers completed device writes to the borrowed image before
 * the host saves it. It does not advance the guest clock. */
/* RC_MEDIA_PROTECT: the write-protect tab of a slot whose file the machine uses in place (a card) moved: `value` 1 protects it
 * at once, 0 lets it be written as the file allows. Returns 1 when the image can be written now, 0 when it cannot (or when the
 * slot is not such a one). */
int rc_machine_control(rc_machine *m, unsigned slot, unsigned command, uint32_t value);
/* The tape recorder of a tape slot (media/tape.h: its tape's blocks, the block and position it is at, whether it
 * runs), for the tape browser; NULL when the slot is not a tape or holds none. */
struct rc_tape_deck;
struct rc_tape_deck *rc_machine_tape_deck(rc_machine *m, unsigned slot);
/* A tape slot's settings (config.h, RC_TAPE_* bits), taken at once: whether the tape is heard, whether the ROM's load and
 * save routines are done by the emulator and whether the waits of loaders are skipped. Returns the bits this machine has
 * a use for (0: the slot is not a tape recorder's, or the machine has none of them); with RC_TAPE_QUERY nothing changes. */
unsigned rc_machine_tape_options(rc_machine *m, unsigned slot, unsigned bits);
/* What auto-load does for a tape just put into tape slot `slot` (typist.h): the text to type, in the harness's notation for
 * `type`, or NULL when there is none (no auto-load); *power_cycle says whether the machine is power cycled before it is
 * typed. A board that knows better than its model's fixed text says so from its state at this moment (a BASIC that was
 * loaded into the RAM must not be lost: no power cycle, the BASIC's own LOAD); the others give the model's `autoload`
 * with a power cycle. */
const char *rc_machine_autoload(const rc_machine *m, unsigned slot, int *power_cycle);

/* --- ports ---------------------------------------------------------------------- */

/* The guest's byte ports (the family's port table). The machine calls
 * `transmit` for every byte the guest sends and `outputs` when its
 * handshake outputs change; bytes from the host go in with
 * rc_machine_port_receive, which returns how many the port took (offer the
 * rest again later), and the host side's modem inputs with
 * rc_machine_port_modem. A parallel port only transmits: its modem call says
 * whether a printer is on line (`dsr`). `loopback` (optional) is asked for
 * each port when the host is set: 1 wires the port's output to its input. */
typedef struct rc_port_host {
    void *ctx;
    void (*transmit)(void *ctx, unsigned port, uint8_t byte);
    void (*outputs)(void *ctx, unsigned port, int dtr, int rts);
    int (*loopback)(void *ctx, unsigned port);
} rc_port_host;
/* NULL: nothing connected (bytes sent are lost). */
void rc_machine_set_ports(rc_machine *m, const rc_port_host *host);
size_t rc_machine_port_receive(rc_machine *m, unsigned port, const uint8_t *data, size_t count);
void rc_machine_port_modem(rc_machine *m, unsigned port, int dcd, int cts, int dsr, int ri);

/* --- states --------------------------------------------------------------------- */

/* The machine's whole state as chunks into w, after the header this writes
 * (state.h): every chip, the memory, the time. Media are not in it. */
void rc_machine_save(const rc_machine *m, rc_state_writer *w);
/* Takes a state back. Refused, with the reason and the machine untouched,
 * when it is of another model or of other ROMs; a state that turns out
 * damaged part-way leaves the machine reset. */
int rc_machine_load(rc_machine *m, rc_state_reader *r, char *error, size_t capacity);

/* --- checks --------------------------------------------------------------------- */

/* A trace of the bus for headless checks and diagnosis: kind RC_TRACE_*
 * with the address and value of each such cycle. NULL stops it. */
enum { RC_TRACE_READ, RC_TRACE_WRITE, RC_TRACE_FETCH, RC_TRACE_INTERRUPT };
typedef void (*rc_trace_fn)(void *ctx, int kind, uint32_t address, uint32_t value);
void rc_machine_set_trace(rc_machine *m, rc_trace_fn fn, void *ctx);
/* A byte of the memory space as the CPU would read it (no side effects),
 * and one written as the CPU would write it. 0 when the machine has no
 * memory at that address. */
int rc_machine_peek(rc_machine *m, uint32_t address, uint8_t *value);
int rc_machine_poke(rc_machine *m, uint32_t address, uint8_t value);

/* Host byte order accepted by the supported targets. */
int rc_machine_host_ok(void);
typedef struct rc_machine_status {
    unsigned drive_present,drive_device,drive_halftrack,drive_led,drive_motor;
    uint64_t tape_counter;unsigned tape_present,tape_motor,tape_play,tape_record;
    unsigned sid_loaded,sid_song,sid_songs,sid_paused;uint64_t sid_cycles;uint32_t sid_hz;
} rc_machine_status;
int rc_machine_get_status(const rc_machine *m,rc_machine_status *status);

#endif /* RC_MACHINE_H */
