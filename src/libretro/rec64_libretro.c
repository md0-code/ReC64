/* Libretro frontend. The board, media and chip state are the same as in the desktop. */
#include <libretro.h>

#include "host_keys.h"
#include "machine.h"
#include "../app/typist.h"
#include "../host/host.h"
#include "rc_version.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define EXTENSIONS "prg|p00|t64|tap|d64|g64|crt|sid"

enum { VIDEO_MAX_W = 1024, VIDEO_MAX_H = 576, AUDIO_BLOCK = 2048, DEFAULT_RATE = 48000 };
enum { STATE_OVERHEAD = 4 * 1024 * 1024 };

static retro_environment_t environment;
static retro_video_refresh_t video;
static retro_audio_sample_t audio_sample;
static retro_audio_sample_batch_t audio_batch;
static retro_input_poll_t input_poll;
static retro_input_state_t input_state;
static retro_log_printf_t log_message;

static rc_machine *machine;
static rc_rom_index roms;
static rc_config config;
static rc_media *media;
static int media_slot = -1;
static rc_typist typist;

static rc_state_writer state;
static size_t state_limit;
static uint8_t content_id[32];
static uint64_t frame_deadline, silent_phase;
static unsigned ports[2] = {RETRO_DEVICE_JOYPAD, RETRO_DEVICE_JOYPAD};
static bool keyboard[RETROK_LAST], tape_play, tape_stop;
static uint8_t sent[256];
static uint32_t pixels[VIDEO_MAX_W * VIDEO_MAX_H];
static struct retro_game_geometry geometry;
static bool keyboard_events, variable_size, failed;

static const char *option(const char *key)
{
    struct retro_variable val = {key, NULL};
    return environment && environment(RETRO_ENVIRONMENT_GET_VARIABLE, &val) ? val.value : NULL;
}

static void error_message(const char *text)
{
    if (log_message)
        log_message(RETRO_LOG_ERROR, "ReC64: %s\n", text);
    if (environment) {
        struct retro_message msg = {text, 240};
        environment(RETRO_ENVIRONMENT_SET_MESSAGE, &msg);
    }
}

static void RETRO_CALLCONV keyboard_event(bool down, unsigned keycode, uint32_t character, uint16_t modifiers)
{
    (void)character;
    (void)modifiers;
    if (keycode < RETROK_LAST)
        keyboard[keycode] = down;
}

static bool held(unsigned keycode)
{
    if (!keycode || keycode >= RETROK_LAST)
        return false;
    return keyboard_events ? keyboard[keycode] :
        input_state && input_state(0, RETRO_DEVICE_KEYBOARD, 0, keycode);
}

static void apply_keyboard(void)
{
    const rc_family *family = rc_machine_family(machine);
    uint8_t next[256] = {0}, combined[256] = {0};
    unsigned count = family->key_count < sizeof next ? family->key_count : (unsigned)sizeof next;
    bool suppress_shift = false;
    bool shift = held(RETROK_LSHIFT) || held(RETROK_RSHIFT);
    for (unsigned i = 0; i < count; ++i)
        next[i] = held(host_key(family->keys[i].host));
    for (unsigned i = 0; i < family->combo_count; ++i) {
        const rc_combo_desc *c = &family->combos[i];
        bool with_shift = strncmp(c->host, "Shift+", 6) == 0;
        if ((with_shift && !shift) || !held(host_key(c->host + (with_shift ? 6 : 0))))
            continue;
        if (c->key < count)
            combined[c->key] = 1;
        if (c->shift < count)
            combined[c->shift] = 1;
        suppress_shift |= with_shift;
    }
    if (suppress_shift) {
        for (unsigned i = 0; i < count; ++i) {
            unsigned keycode = host_key(family->keys[i].host);
            if (keycode == RETROK_LSHIFT || keycode == RETROK_RSHIFT)
                next[i] = 0;
        }
    }
    bool any = false;
    for (unsigned i = 0; i < count; ++i) {
        next[i] |= combined[i];
        any |= next[i] != 0;
    }
    if (any && rc_typist_busy(&typist))
        rc_typist_cancel(&typist);
    for (unsigned i = 0; i < count; ++i) {
        if (next[i] != sent[i])
            rc_machine_key(machine, i, next[i]);
    }
    memcpy(sent, next, count);
}

static bool pad(unsigned port, unsigned id)
{
    return input_state && ports[port] == RETRO_DEVICE_JOYPAD &&
        input_state(port, RETRO_DEVICE_JOYPAD, 0, id);
}

static bool tape_mounted(void)
{
    return media && media_slot >= 0 && (rc_machine_family(machine)->media[media_slot].kind == RC_MEDIA_TAPE ||
        rc_machine_family(machine)->media[media_slot].kind == RC_MEDIA_PROGRAM);
}

static void apply_input(void)
{
    if (input_poll)
        input_poll();
    apply_keyboard();
    const rc_family *family = rc_machine_family(machine);
    static const struct { unsigned pad, signal; } controls[] = {
        {RETRO_DEVICE_ID_JOYPAD_UP, RC_JOY_UP}, {RETRO_DEVICE_ID_JOYPAD_DOWN, RC_JOY_DOWN},
        {RETRO_DEVICE_ID_JOYPAD_LEFT, RC_JOY_LEFT}, {RETRO_DEVICE_ID_JOYPAD_RIGHT, RC_JOY_RIGHT},
        {RETRO_DEVICE_ID_JOYPAD_B, RC_JOY_FIRE}, {RETRO_DEVICE_ID_JOYPAD_A, RC_JOY_FIRE2}
    };
    for (unsigned port = 0; port < family->joysticks && port < 2; ++port) {
        unsigned bits = 0;
        for (size_t i = 0; i < sizeof controls / sizeof controls[0]; ++i)
            if (pad(port, controls[i].pad))
                bits |= controls[i].signal;
        rc_machine_joystick(machine, port ^ 1u, bits);
    }
    bool play = pad(0, RETRO_DEVICE_ID_JOYPAD_L);
    bool stop = pad(0, RETRO_DEVICE_ID_JOYPAD_R);
    if (tape_mounted()) {
        if (play && !tape_play)
            rc_machine_control(machine, (unsigned)media_slot, RC_MEDIA_PLAY, 0);
        if (stop && !tape_stop)
            rc_machine_control(machine, (unsigned)media_slot, RC_MEDIA_STOP, 0);
    }
    tape_play = play;
    tape_stop = stop;
}

static bool picture(rc_frame *f)
{
    if (!machine || !rc_machine_frame(machine, f) || !f->pixels)
        return false;

    return f->width && f->height && f->width <= VIDEO_MAX_W && f->height <= VIDEO_MAX_H;
}

static struct retro_game_geometry frame_geometry(const rc_frame *f)
{
    return (struct retro_game_geometry){f->width, f->height, VIDEO_MAX_W, VIDEO_MAX_H,
        f->aspect_h ? (float)f->aspect_w / (float)f->aspect_h : 0.0f};
}

static void send_picture(void)
{
    rc_frame f;
    if (!video || !picture(&f))
        return;
    struct retro_game_geometry next = frame_geometry(&f);
    if (next.base_width != geometry.base_width || next.base_height != geometry.base_height ||
        next.aspect_ratio != geometry.aspect_ratio) {
        geometry = next;
        if (environment)
            environment(RETRO_ENVIRONMENT_SET_GEOMETRY, &geometry);
    }
    rc_frame_argb(&f, pixels);
    video(pixels, f.width, f.height, (size_t)f.width * sizeof pixels[0]);
}

static void send_audio(const int16_t *samples, size_t count)
{
    if (audio_batch)
        audio_batch(samples, count);
    else if (audio_sample)
        for (size_t i = 0; i < count; ++i)
            audio_sample(samples[i * 2], samples[i * 2 + 1]);
}

static void pull_audio(void)
{
    int16_t samples[2 * AUDIO_BLOCK];
    if (!rc_machine_audio_rate(machine)) {
        memset(samples, 0, sizeof samples);
        silent_phase += DEFAULT_RATE * rc_machine_frame_period(machine);
        uint64_t count = silent_phase / rc_machine_clock(machine);
        silent_phase %= rc_machine_clock(machine);
        while (count) {
            size_t n = count > AUDIO_BLOCK ? AUDIO_BLOCK : (size_t)count;
            send_audio(samples, n);
            count -= n;
        }
    } else {
        size_t n;
        while ((n = rc_machine_audio_pull(machine, samples, AUDIO_BLOCK)) != 0)
            send_audio(samples, n);
    }
}

static const struct retro_variable options[] = {
    {"rec64_model", "Machine model (reload); c64-pal|c64c-pal|c64-ntsc|c64c-ntsc"},
    {"rec64_sid", "SID model (reload); 6581|8580"},
    {"rec64_autoload", "Auto-load content (reload); enabled|disabled"},
    {"rec64_border", "Screen border (reload); full|medium|minimal|none"},
    {NULL, NULL}
};

static void register_options(void)
{
    environment(RETRO_ENVIRONMENT_SET_VARIABLES, (void *)options);
}

static bool firmware_and_options(char *error, size_t capacity)
{
    const char *dir = NULL;
    if (!environment(RETRO_ENVIRONMENT_GET_SYSTEM_DIRECTORY, &dir) || !dir || !dir[0]) {
        snprintf(error, capacity, "Set the frontend's system directory and put the ROMs in its rec64 folder.");
        return false;
    }
    char folder[RC_ROM_PATH];
    if (snprintf(folder, sizeof folder, "%s/rec64", dir) >= (int)sizeof folder) {
        snprintf(error, capacity, "The frontend's system directory path is too long.");
        return false;
    }
    const char *model = option("rec64_model");
    if (model) {
        if (rc_family_of_model(model, NULL) != rc_family_of_model(RC_CONFIG_DEFAULT_MODEL, NULL)) {
            snprintf(error, capacity, "Unknown ReC64 model: %.100s.", model);
            return false;
        }
        snprintf(config.model, sizeof config.model, "%s", model);
    }
    const char *sid = option("rec64_sid");
    if (sid)
        rc_config_set_option(&config, "sid", sid);
    const char *border = option("rec64_border");
    static const char *const borders[] = {
        [RC_BORDER_FULL] = "full", [RC_BORDER_MEDIUM] = "medium",
        [RC_BORDER_MINIMAL] = "minimal", [RC_BORDER_NONE] = "none"
    };
    for (unsigned i = 0; border && i < sizeof borders / sizeof borders[0]; ++i)
        if (!strcmp(border, borders[i]))
            config.border = i;
    const char *folders[] = {folder};
    if (!rc_rom_index_scan(&roms, folders, sizeof folders / sizeof folders[0])) {
        snprintf(error, capacity, "Cannot index firmware in %.300s.", folder);
        return false;
    }
    const char *required[] = {"basic", "kernal", "chargen"};
    for (unsigned i = 0; i < sizeof required / sizeof required[0]; ++i) {
        const rc_known_rom *rom = rc_romdb_find(required[i]);
        if (!rom || !rc_rom_index_find(&roms, rom)) {
            snprintf(error, capacity, "Missing %s firmware in the system/rec64 folder.", required[i]);
            return false;
        }
    }
    if (!rc_rom_index_find(&roms, rc_romdb_find("dos1541")) &&
        !rc_rom_index_find(&roms, rc_romdb_find("dos1541ii"))) {
        snprintf(error, capacity, "Put a 1541 or 1541-II drive ROM in the system/rec64 folder.");
        return false;
    }
    return true;
}

static rc_machine *create_machine(char *error, size_t capacity)
{
    rc_machine *new_machine = rc_machine_create(&config, &roms, error, capacity);
    if (new_machine)
        rc_machine_border(new_machine, config.border);
    return new_machine;
}

static bool load_content(const char *path, char *error, size_t capacity)
{
    if (strlen(path) >= RC_MEDIA_PATH) {
        snprintf(error, capacity, "Content path is too long.");
        return false;
    }
    const rc_family *family = rc_machine_family(machine);
    for (unsigned i = 0; i < family->media_count; ++i) {
        if (!rc_machine_has_media(machine, i) || !rc_media_extension_in(path, family->media[i].extensions))
            continue;
        media = rc_media_open(path, 1, error, capacity);
        if (!media || !rc_machine_insert(machine, i, media, error, capacity))
            return false;
        media_slot = (int)i;
        rc_sha256(media->data, media->size, content_id);
        return true;
    }
    snprintf(error, capacity, "The selected model has no drive for this content.");
    return false;
}

static bool start_content(char *error, size_t capacity)
{
    if (!media)
        return true;
    const rc_family *family = rc_machine_family(machine);
    unsigned slot = (unsigned)media_slot;
    if (family->media[slot].kind == RC_MEDIA_CART || family->media[slot].kind == RC_MEDIA_SID) {
        rc_machine_reset(machine, RC_RESET_COLD);
        return true;
    }
    const char *autoload = option("rec64_autoload");
    if (autoload && !strcmp(autoload, "disabled"))
        return true;
    int power = 0;
    const char *commands = rc_machine_autoload(machine, slot, &power);
    if (!commands || !*commands)
        return true;
    char command_text[sizeof typist.commands];
    snprintf(command_text, sizeof command_text, "%s", commands);
    if (power)
        rc_machine_reset(machine, RC_RESET_AUTOLOAD);
    return rc_typist_start(&typist, command_text, error, capacity) != 0;
}

static rc_media *copy_media(const rc_media *image, char *error, size_t capacity)
{
    (void)error;
    (void)capacity;
    rc_media *copy = rc_media_from(image->path, image->data, image->size);
    if (copy)
        copy->writable = 0;
    return copy;
}

unsigned retro_api_version(void) { return RETRO_API_VERSION; }

void retro_set_environment(retro_environment_t cb)
{
    environment = cb;
    if (cb) {
        bool yes = true;
        cb(RETRO_ENVIRONMENT_SET_SUPPORT_NO_GAME, &yes);
        register_options();
    }
}

void retro_set_video_refresh(retro_video_refresh_t cb) { video = cb; }
void retro_set_audio_sample(retro_audio_sample_t cb) { audio_sample = cb; }
void retro_set_audio_sample_batch(retro_audio_sample_batch_t cb) { audio_batch = cb; }
void retro_set_input_poll(retro_input_poll_t cb) { input_poll = cb; }
void retro_set_input_state(retro_input_state_t cb) { input_state = cb; }

void retro_init(void)
{
    struct retro_log_callback logger = {0};
    log_message = environment && environment(RETRO_ENVIRONMENT_GET_LOG_INTERFACE, &logger) ? logger.log : NULL;
    struct retro_keyboard_callback keys = {keyboard_event};
    keyboard_events = environment && environment(RETRO_ENVIRONMENT_SET_KEYBOARD_CALLBACK, &keys);
    uint64_t quirks = RETRO_SERIALIZATION_QUIRK_CORE_VARIABLE_SIZE;
    variable_size = environment && environment(RETRO_ENVIRONMENT_SET_SERIALIZATION_QUIRKS, &quirks) &&
        (quirks & RETRO_SERIALIZATION_QUIRK_FRONT_VARIABLE_SIZE);
    rc_sw_init(&state);
}

void retro_get_system_info(struct retro_system_info *info)
{
    *info = (struct retro_system_info){
        .library_name = "ReC64", .library_version = RC_VERSION, .valid_extensions = EXTENSIONS,
        .need_fullpath = true, .block_extract = false
    };
}

void retro_get_system_av_info(struct retro_system_av_info *info)
{
    rc_frame f;
    geometry = (struct retro_game_geometry){320, 240, VIDEO_MAX_W, VIDEO_MAX_H, 4.0f / 3.0f};
    if (picture(&f))
        geometry = frame_geometry(&f);
    info->geometry = geometry;
    info->timing.fps = machine ? (double)rc_machine_clock(machine) / (double)rc_machine_frame_period(machine) : 50.0;
    unsigned rate = machine ? rc_machine_audio_rate(machine) : 0;
    info->timing.sample_rate = rate ? rate : DEFAULT_RATE;
}

void retro_set_controller_port_device(unsigned port, unsigned device)
{
    if (port < 2)
        ports[port] = device & RETRO_DEVICE_MASK;
}

static void configure_controls(void)
{
    if (!environment)
        return;
    static const struct retro_controller_description types[] = {{"RetroPad", RETRO_DEVICE_JOYPAD}};
    static const struct retro_controller_info devices[] = {{types, 1}, {types, 1}, {NULL, 0}};
    environment(RETRO_ENVIRONMENT_SET_CONTROLLER_INFO, (void *)devices);
    static struct retro_input_descriptor desc[] = {
        {0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_B, "Control port 2 Fire"},
        {0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_A, "Second fire"},
        {1, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_B, "Control port 1 Fire"},
        {0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_L, "Play tape"},
        {0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_R, "Stop tape"},
        {0, 0, 0, 0, NULL}
    };
    desc[3].description = tape_mounted() ? "Play tape" : NULL;
    environment(RETRO_ENVIRONMENT_SET_INPUT_DESCRIPTORS, desc);
}

void retro_unload_game(void)
{
    if (machine)
        rc_machine_destroy(machine);
    machine = NULL;

    rc_media_close(media);
    media = NULL;
    media_slot = -1;
    rc_rom_index_free(&roms);
    rc_sw_free(&state);
    state_limit = 0;
    failed = false;
    frame_deadline = silent_phase = 0;
    tape_play = tape_stop = false;
    memset(keyboard, 0, sizeof keyboard);
    memset(sent, 0, sizeof sent);
    memset(content_id, 0, sizeof content_id);
}

void retro_deinit(void) { retro_unload_game(); }

bool retro_load_game(const struct retro_game_info *game)
{
    retro_unload_game();
    enum retro_pixel_format format = RETRO_PIXEL_FORMAT_XRGB8888;
    if (!environment || !environment(RETRO_ENVIRONMENT_SET_PIXEL_FORMAT, &format)) {
        error_message("The frontend must support XRGB8888 video.");
        return false;
    }
    char error[512] = "";
    rc_config_defaults(&config);
    rc_rom_index_init(&roms);
    rc_typist_init(&typist);
    if (!firmware_and_options(error, sizeof error))
        goto fail;
    machine = create_machine(error, sizeof error);
    if (!machine)
        goto fail;

    if (game && (!game->path || !game->path[0])) {
        snprintf(error, sizeof error, "The frontend did not give a content path.");
        goto fail;
    }
    if (game && !load_content(game->path, error, sizeof error))
        goto fail;
    if (!start_content(error, sizeof error))
        goto fail;
    configure_controls();
    state_limit = STATE_OVERHEAD + (media ? 4 * media->size : 0);
    frame_deadline = rc_machine_time(machine);
    return true;

fail:
    error_message(error[0] ? error : "Could not start the selected machine.");
    retro_unload_game();
    return false;
}

bool retro_load_game_special(unsigned game_type, const struct retro_game_info *info, size_t num_info)
{
    (void)game_type;
    (void)info;
    (void)num_info;
    return false;
}

void retro_reset(void)
{
    if (!machine)
        return;
    rc_typist_cancel(&typist);
    rc_machine_reset(machine, RC_RESET_COLD);

    memset(sent, 0, sizeof sent);
    char error[256] = "";
    if (!start_content(error, sizeof error))
        error_message(error);
    frame_deadline = rc_machine_time(machine);
    silent_phase = 0;
    failed = false;
}

void retro_run(void)
{
    if (!machine)
        return;
    if (!failed) {
        apply_input();
        if (rc_typist_frame(&typist, machine, NULL) < 0)
            error_message(typist.error);
        frame_deadline += rc_machine_frame_period(machine);
        uint64_t now = rc_machine_time(machine);
        if (now < frame_deadline && rc_machine_run(machine, frame_deadline - now) != RC_RUN_OK) {
            error_message(rc_machine_error(machine));
            failed = true;
        }
    }
    send_picture();
    pull_audio();
}

unsigned retro_get_region(void)
{
    return machine && (uint64_t)rc_machine_clock(machine) > 55 * rc_machine_frame_period(machine) ?
        RETRO_REGION_NTSC : RETRO_REGION_PAL;
}

static bool save_state(void)
{
    state.size = state.chunk = 0;
    state.failed = 0;
    rc_machine_save(machine, &state);
    rc_sw_begin(&state, "LRTR", 0);
    rc_sw_u32(&state, 1);
    rc_sw_bytes(&state, content_id, sizeof content_id);
    rc_sw_u64(&state, frame_deadline);
    rc_sw_u64(&state, silent_phase);
    rc_typist_save(&typist, &state);
    rc_sw_end(&state);
    return rc_sw_finish(&state) != 0;
}

size_t retro_serialize_size(void)
{
    if (!machine)
        return 0;
    if (!variable_size)
        return state_limit;
    return save_state() ? state.size + 4 : 0;
}

bool retro_serialize(void *data, size_t size)
{
    if (!machine || !data || size < 4 || (!variable_size && size < state_limit) ||
        !save_state() || state.size > size - 4 || state.size > UINT32_MAX)
        return false;
    uint8_t *out = data;
    for (unsigned i = 0; i < 4; ++i)
        out[i] = (uint8_t)(state.size >> (8 * i));
    memcpy(out + 4, state.data, state.size);
    memset(out + 4 + state.size, 0, size - 4 - state.size);
    return true;
}

bool retro_unserialize(const void *data, size_t size)
{
    if (!machine || !data || size < 4)
        return false;
    const uint8_t *in = data;
    size_t length = (uint32_t)in[0] | (uint32_t)in[1] << 8 | (uint32_t)in[2] << 16 | (uint32_t)in[3] << 24;
    if (length > size - 4)
        return false;
    char error[512] = "";
    rc_state_reader reader;
    uint8_t id[sizeof content_id];
    if (!rc_sr_open(&reader, in + 4, length, error, sizeof error) || !rc_sr_chunk(&reader, "LRTR", 0) ||
        rc_sr_u32(&reader) != 1)
        return false;
    rc_sr_bytes(&reader, id, sizeof id);
    uint64_t deadline = rc_sr_u64(&reader);
    uint64_t phase = rc_sr_u64(&reader);
    rc_typist next_typist;
    if (memcmp(content_id, id, sizeof id) || phase >= rc_machine_clock(machine) || !rc_typist_load(&next_typist, &reader))
        return false;

    rc_machine *next = create_machine(error, sizeof error);
    if (!next)
        return false;
    rc_media *next_media = NULL;
    if (media) {
        next_media = copy_media(media, error, sizeof error);
        if (!next_media || !rc_machine_insert(next, (unsigned)media_slot, next_media, error, sizeof error))
            goto invalid;
    }
    if (!rc_machine_load(next, &reader, error, sizeof error))
        goto invalid;
    uint64_t now = rc_machine_time(next);
    uint64_t period = rc_machine_frame_period(next);
    if (deadline > now ? deadline - now > period : now - deadline > period * 32u)
        goto invalid;
    rc_machine_destroy(machine);
    rc_media_close(media);
    machine = next;
    media = next_media;
    typist = next_typist;
    frame_deadline = deadline;
    silent_phase = phase;
    memset(sent, 0, sizeof sent);
    const rc_family *family = rc_machine_family(machine);
    for (unsigned i = 0; i < family->key_count; ++i)
        rc_machine_key(machine, i, 0);
    failed = false;
    return true;

invalid:
    rc_machine_destroy(next);
    rc_media_close(next_media);
    return false;
}

void retro_cheat_reset(void) { }
void retro_cheat_set(unsigned index, bool enabled, const char *code)
{
    (void)index;
    (void)enabled;
    (void)code;
}

void *retro_get_memory_data(unsigned id) { (void)id; return NULL; }
size_t retro_get_memory_size(unsigned id) { (void)id; return 0; }
