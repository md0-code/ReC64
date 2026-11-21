/* rec64.yaml: the machine and its table-defined options and media, plus
 * host display, sound, controllers and UI settings. libyaml reads syntax;
 * config.c rejects unknown or duplicate keys and yamledit preserves comments.
 * Core code includes only portable interfaces, never SDL or OS headers. */
#ifndef RC_CONFIG_H
#define RC_CONFIG_H

#include <stddef.h>
#include <stdint.h>

#define RC_CONFIG_PATH 1024
#define RC_CONFIG_NAME 32               /* a model id, an option's key or value, a slot's key, with its end */
#define RC_CONFIG_OPTIONS 32
#define RC_CONFIG_MEDIA 8
#define RC_CONFIG_PORTS 8

/* video.look */
enum { RC_LOOK_PLAIN, RC_LOOK_CRT };
/* video.aspect, video.scale, video.filter */
enum { RC_CONFIG_ASPECT_ORIGINAL, RC_CONFIG_ASPECT_SQUARE };
enum { RC_CONFIG_SCALE_FIT, RC_CONFIG_SCALE_INTEGER };
enum { RC_CONFIG_FILTER_NEAREST, RC_CONFIG_FILTER_LINEAR, RC_CONFIG_FILTER_SHARP };
/* ui.language: the language of the setup screen and of the messages shown
 * in the window (text.h); the command line's help and the diagnostics on
 * stderr stay English. */
enum { RC_LANG_EN, RC_LANG_COUNT };

/* controllers.joystickN.device: what feeds a joystick socket of the machine
 * (how many it has is the family's, machine.h; a socket the model does not
 * have is ignored): nothing, the keyboard
 * joystick (the keys below) or a host game controller by its number (the
 * order the host reports them). What kind of joystick is in the socket
 * (Kempston, Sinclair, Cursor...) is an option of the machine, not here. */
#define RC_CONFIG_STICKS 2
#define RC_CONFIG_PADS 2                /* host game controllers numbered 1 and 2 */
enum { RC_STICK_NONE, RC_STICK_KEYBOARD, RC_STICK_CONTROLLER, RC_STICK_MOUSE };
/* controllers.keys: the keyboard joystick's keys, in this order */
enum { RC_STICK_UP, RC_STICK_DOWN, RC_STICK_LEFT, RC_STICK_RIGHT, RC_STICK_FIRE, RC_STICK_FIRE2, RC_CONFIG_STICK_KEYS };
#define RC_CONFIG_KEY_NAME 32           /* a host key's name, with its end */

enum { RC_CONTROL_JOYSTICK, RC_CONTROL_MOUSE1351, RC_CONTROL_PADDLES, RC_CONTROL_NONE };
typedef struct rc_config_binding {char key[32],host[32];} rc_config_binding;
#define RC_CONFIG_BINDINGS 96
typedef struct rc_config_stick {
    uint32_t device;                    /* RC_STICK_* */
    uint32_t type;                      /* RC_CONTROL_* */
    uint32_t pad;                       /* RC_STICK_CONTROLLER: the controller, 1 to RC_CONFIG_PADS */
} rc_config_stick;

typedef struct rc_config_option {
    char key[RC_CONFIG_NAME], value[RC_CONFIG_NAME];
} rc_config_option;

enum {RC_MEDIA_SETTING_FAST=1,RC_MEDIA_SETTING_SOUND=2,RC_MEDIA_SETTING_AUTOLOAD=4,RC_MEDIA_SETTING_FULL_SPEED=8,
      RC_MEDIA_SETTING_TRAPS=16,RC_MEDIA_SETTING_SAVE_TRAPS=32,RC_MEDIA_SETTING_ACCELERATE=64,RC_MEDIA_SETTING_PROTECT=128};
typedef struct rc_config_media {
    char key[RC_CONFIG_NAME];
    char image[RC_CONFIG_PATH];         /* empty: an empty slot */
    int write_protect;
    int startup;                        /* cartridge: keep this image inserted on launch */
    int fast;                           /* not original: the shell runs at full speed while this slot works; a tape is also
                                         * loaded and saved by the fast means below */
    /* Media playback and loading. */
    int sound;                          /* tape signal or synthesised drive mechanism */
    int autoload;                       /* an image put into the slot is loaded at once (typist.h) */
    /* The parts of a tape's `fast`, each on in a new configuration and of effect only while `fast` is on (the setup screen
     * has the one switch; the file may take a part out): the shell's full speed, the ROM's load and save routines done at
     * once (hw/traps.h), the waits of other loaders skipped (hw/loadaccel.h). */
    int full_speed, traps, save_traps, accelerate;
    unsigned explicit_flags;            /* keys actually supplied, for shared tape defaults */
} rc_config_media;
/* Media options applied to the machine. Fast loading mutes media sound.
 * NULL uses the default fast settings. */
enum { RC_TAPE_SOUND = 1, RC_TAPE_TRAPS = 2, RC_TAPE_SAVE_TRAPS = 4, RC_TAPE_ACCELERATE = 8 };
#define RC_TAPE_QUERY 0x80000000u       /* instead of settings: nothing changes, the machine only says which it has */
unsigned rc_config_tape_bits(const rc_config_media *media);

/* ports.KEY: the host end of a guest byte port (a family's port table, machine.h). The file words an end as none (the
 * default), loopback (a serial port's output wired back to its input), a host serial device (COM3 or serial:COM3),
 * pipe:NAME, tcp:HOST:PORT (a client), tcp-listen:[HOST:]PORT (a server) or file:PATH (what the guest sends is appended
 * to the file); a printer takes every end but loopback. A key the running model has no port for is kept and ignored, so
 * switching models loses nothing; a key no model has is refused. `path` is the device, the pipe's name, HOST:PORT, the
 * port or the file (relative to the configuration's folder in the file, as media are). */
enum { RC_END_NONE, RC_END_LOOPBACK, RC_END_SERIAL, RC_END_PIPE, RC_END_FILE, RC_END_TCP, RC_END_TCP_LISTEN, RC_END_KINDS };
typedef struct rc_config_port {
    char key[RC_CONFIG_NAME];
    uint32_t type;                      /* RC_END_* */
    char path[RC_CONFIG_PATH];
} rc_config_port;

typedef struct rc_config {
    /* machine */
    char model[RC_CONFIG_NAME];
    rc_config_option options[RC_CONFIG_OPTIONS];
    unsigned option_count;
    /* media */
    rc_config_media media[RC_CONFIG_MEDIA];
    unsigned media_count;
    char media_folder[RC_CONFIG_PATH]; /* ui.media_folder: initial load/save folder */
    /* ports: the ends the file names, for any model */
    rc_config_port ports[RC_CONFIG_PORTS];
    unsigned port_count;
    /* video */
    uint32_t look, aspect, scale, filter;
    uint32_t border;                    /* full, sixteen-pixel margin, no border */
    char palette[128];                  /* default or color-corrected */
    int fullscreen, vsync;
    /* sound: enabled is the mute switch, volume 0 to 100 percent */
    int sound, volume;
    /* controllers: the sockets' devices; the keyboard joystick's keys by SDL's key names (what the
     * window's keys leave free: rc_config_stick_key_problem); whether those keys feed the joystick
     * from the start (the shell's F11 key switches them at any time) */
    rc_config_stick sticks[RC_CONFIG_STICKS];
    char stick_keys[RC_CONFIG_STICK_KEYS][RC_CONFIG_KEY_NAME];
    int keyboard_joystick;
    unsigned keyboard_port;             /* fallback socket, 0 or 1 */
    rc_config_binding bindings[RC_CONFIG_BINDINGS];
    unsigned binding_count;
    /* ui */
    uint32_t language;                  /* RC_LANG_*; English only */
    int state_slot;                     /* the slot Ctrl+F5 saves to and Ctrl+F6 loads from, 1 to 10 */
    int status_line;                    /* ui.status_line: the status line is shown (F9 switches it) */
    char setup_page[RC_CONFIG_NAME];    /* the page of the setup screen that was open last ("media", "machine"...): F12 opens it again */
    /* the file this was read from (relative paths in it are relative to its folder) */
    char source[RC_CONFIG_PATH];
} rc_config;

/* The model a new configuration runs. */
#define RC_CONFIG_DEFAULT_MODEL "c64-pal"

void rc_config_defaults(rc_config *config);
/* The socket the keyboard joystick drives: keyboard_port when it holds a joystick, else the other
 * socket when that one does; RC_CONFIG_STICKS when neither does. */
unsigned rc_config_keyboard_socket(const rc_config *config);
/* Reads a file; 0 with a message naming the line when it is not valid. */
int rc_config_load(const char *path, rc_config *config, char *error, size_t capacity);
/* The same from text; source is the file it stands for (relative paths). */
int rc_config_parse(const char *text, size_t length, const char *source, rc_config *config, char *error, size_t capacity);

/* Every setting of config written into doc (yamledit.h) at its key: only
 * values change, comments and order stay. Paths inside the configuration's
 * folder are written relative to it. 0 when out of memory. */
struct rc_ydoc;
int rc_config_store(const rc_config *config, struct rc_ydoc *doc);

/* The value of a machine option as the file words it; NULL when the
 * configuration does not set it (the option is then at its initial value). */
const char *rc_config_option_value(const rc_config *config, const char *key);
/* Sets it (adds the key when missing); 0 when the table is full. */
int rc_config_set_option(rc_config *config, const char *key, const char *value);
/* The settings of a media slot; NULL when the configuration has none.
 * rc_config_media_slot adds an empty one when missing (NULL: table full). */
const rc_config_media *rc_config_media_find(const rc_config *config, const char *key);
rc_config_media *rc_config_media_slot(rc_config *config, const char *key);
/* Effective settings; the virtual program slot inherits unspecified tape controls. */
void rc_config_media_settings(const rc_config *config,const char *key,rc_config_media *out);

/* The end of a port by its key; NULL when the configuration has none (the port is then not connected). The slot form adds
 * an unconnected one when missing (NULL: the table is full). */
const rc_config_port *rc_config_port_find(const rc_config *config, const char *key);
rc_config_port *rc_config_port_slot(rc_config *config, const char *key);
/* An end as the file words it: "none", "loopback", "COM3", "pipe:NAME", "file:out.txt" (a path inside the configuration's
 * folder relative to it), "tcp:HOST:PORT", "tcp-listen:PORT". */
const char *rc_config_port_text(const rc_config *config, const rc_config_port *port, char *buffer, size_t capacity);
/* Whether `target` suits an end of that type: a TCP client needs HOST:PORT, a server [HOST:]PORT, the port 1 to 65535 (0 is
 * allowed for a server: any free port), the others a name that is not empty. RC_PORT_OK or what is wrong; the second gives it in
 * words. */
enum { RC_PORT_OK, RC_PORT_NEEDS_HOST_PORT, RC_PORT_BAD_TCP_PORT, RC_PORT_LISTEN_FORM, RC_PORT_EMPTY };
int rc_config_port_problem(uint32_t type, const char *target);
const char *rc_config_port_problem_text(int problem);

/* The keyboard joystick's keys. The file's name of a function ("up", "fire2") and its default key
 * ("Up", "Left Alt"). */
const char *rc_config_stick_key_name(unsigned function);
const char *rc_config_stick_key_default(unsigned function);
/* Socket input: none, controller 1/2, or mouse for paddles. */
const char *rc_config_stick_device_name(const rc_config_stick *stick, char *buffer, size_t capacity);
/* The shell tells the reader which key names the host knows (SDL's, which the core does not link):
 * without one every printable name is taken. */
void rc_config_set_key_check(int (*known)(const char *name));
/* Whether `name` can be the key of `function`: RC_KEY_NAME_OK; _BAD when the host has no key of that
 * name; _RESERVED when the window keeps it (F9 to F12, Pause, Print Screen; *reserved is how it is
 * written); _USED when another function of the keyboard joystick has it (*other). */
enum { RC_KEY_NAME_OK, RC_KEY_NAME_BAD, RC_KEY_NAME_RESERVED, RC_KEY_NAME_USED };
int rc_config_stick_key_problem(const rc_config *config, unsigned function, const char *name, const char **reserved, unsigned *other);

/* The names the file uses for the choice values ("crt", "sharp"). */
const char *rc_config_look_name(uint32_t look);
const char *rc_config_aspect_name(uint32_t aspect);
const char *rc_config_scale_name(uint32_t scale);
const char *rc_config_filter_name(uint32_t filter);
const char *rc_config_language_name(uint32_t language); /* "en" */
/* The language a host locale name stands for ("ro-RO", "ro_RO.UTF-8"):
 * English for every locale. */
uint32_t rc_config_language_for_locale(const char *locale);
/* The language of the host's user locale (host.h); English when it is unknown. */
uint32_t rc_config_default_language(void);

#endif /* RC_CONFIG_H */
