/* Host sound output (
 * interface): SDL queues stereo at the device's rate; audiosync.c converts the
 * machine's rate to it and keeps the queue short by following the device's
 * clock. */
#include "audio.h"

#include <string.h>

int rc_host_audio_open(rc_host_audio *a) {
    memset(a, 0, sizeof *a);
    SDL_AudioSpec want = {0};
    want.freq = 48000; want.format = AUDIO_S16SYS; want.channels = 2; want.samples = 512;
    a->device = SDL_OpenAudioDevice(NULL, 0, &want, &a->output, SDL_AUDIO_ALLOW_FREQUENCY_CHANGE);
    if (!a->device) return 0;
    SDL_PauseAudioDevice(a->device, 0);
    return 1;
}

#define CHUNK 512u              /* machine frames pulled at once */
#define OUT_FRAMES 4096u        /* what a chunk can make at the device's rate (down to 1/8 of the machine's) */
/* Queue `frames` stereo frames of silence. */
static int queue_silence(rc_host_audio *a, size_t frames) {
    int16_t zeros[512 * 2];
    memset(zeros, 0, sizeof zeros);
    while (frames) {
        const size_t n = frames < 512 ? frames : 512;
        if (SDL_QueueAudio(a->device, zeros, (Uint32)(n * 4))) return 0;
        frames -= n;
    }
    return 1;
}

int rc_host_audio_pump(rc_host_audio *a, rc_machine *m, unsigned volume, int discard, int racing) {
    const uint32_t rate = rc_machine_audio_rate(m);
    int16_t samples[CHUNK * 2];
    if (!rate) return 1;
    if (!a->device || discard) {   /* no output: keep the machine's buffer empty */
        while (rc_machine_audio_pull(m, samples, CHUNK) == CHUNK) {}
        if (a->device && a->flowing) {
            SDL_ClearQueuedAudio(a->device);
            a->flowing = 0;
        }
        return 1;
    }
    if (a->rate != rate) {
        rc_audiosync_init(&a->sync, rate, (uint32_t)a->output.freq);
        a->rate = rate;
        a->flowing = 0;
    }
    size_t queued = SDL_GetQueuedAudioSize(a->device) / 4u;
    if (!a->flowing || queued == 0) {
        /* The start, or the device ran dry (a stall): a cushion of silence puts the queue at its target again. */
        if (!queue_silence(a, rc_audiosync_refill(&a->sync, queued))) return 0;
        queued = SDL_GetQueuedAudioSize(a->device) / 4u;
        a->flowing = 1;
    }
    rc_audiosync_observe(&a->sync, queued);
    const unsigned gain = volume >= 100 ? 256u : volume * 256u / 100u;
    int16_t out[OUT_FRAMES * 2];
    size_t frames;
    while ((frames = rc_machine_audio_pull(m, samples, CHUNK)) > 0) {
        const size_t pulled = frames;
        /* A machine ahead of the clock makes more sound than the device plays: only what tops the queue up to its
         * target is kept, so the queue is no longer than usual when the machine falls back to the clock. */
        if (racing) frames = rc_audiosync_wanted(&a->sync, queued, frames);
        /* The device stopped consuming (a long stall): input is dropped rather than queued without end. */
        else if ((double)queued > a->sync.target * RC_AUDIOSYNC_OVERFLOW) frames = 0;
        if (frames) {
            if (rc_audiosync_capacity(&a->sync, frames) > OUT_FRAMES) return 0;
            const size_t n = rc_audiosync_convert(&a->sync, samples, frames, out, gain);
            if (n && SDL_QueueAudio(a->device, out, (Uint32)(n * 4))) return 0;
            queued += n;
        }
        if (pulled < CHUNK) break;
    }
    return 1;
}

void rc_host_audio_close(rc_host_audio *a) {
    if (a->device) SDL_CloseAudioDevice(a->device);
    memset(a, 0, sizeof *a);
}

/* --- WAV recording ------------------------------------------------------------- */

static void le(uint8_t *p, uint32_t v, int n) { for (int i = 0; i < n; i++) p[i] = (uint8_t)(v >> (8 * i)); }

static int wav_header(rc_wav *w) {
    uint8_t h[44];
    memset(h, 0, sizeof h);
    memcpy(h, "RIFF", 4);
    le(h + 4, 36u + w->frames * 4u, 4);
    memcpy(h + 8, "WAVEfmt ", 8);
    le(h + 16, 16, 4);
    le(h + 20, 1, 2);              /* PCM */
    le(h + 22, 2, 2);              /* stereo */
    le(h + 24, w->rate, 4);
    le(h + 28, w->rate * 4u, 4);
    le(h + 32, 4, 2);
    le(h + 34, 16, 2);
    memcpy(h + 36, "data", 4);
    le(h + 40, w->frames * 4u, 4);
    return !fseek(w->file, 0, SEEK_SET) && fwrite(h, 1, sizeof h, w->file) == sizeof h && !fseek(w->file, 0, SEEK_END);
}

int rc_wav_open(rc_wav *w, const char *path) {
    memset(w, 0, sizeof *w);
    w->file = fopen(path, "wb");
    if (!w->file) return 0;
    if (wav_header(w)) return 1;
    fclose(w->file); w->file = NULL; return 0;
}

void rc_wav_pump(rc_wav *w, rc_machine *m, int muted) {
    int16_t buf[2048 * 2];
    size_t n;
    if (!w->file) return;
    if (!w->rate) w->rate = rc_machine_audio_rate(m);
    while ((n = rc_machine_audio_pull(m, buf, 2048)) > 0) {
        if (muted) memset(buf, 0, n * 2 * sizeof *buf);
        for (size_t i = 0; i < 2 * n; i++) {
            const int v = buf[i] < 0 ? -buf[i] : buf[i];
            if (v > w->peak) w->peak = v;
        }
        /* WAV samples are little-endian. */
        uint8_t out[2048 * 4];
        for (size_t i = 0; i < 2 * n; i++) le(out + 2 * i, (uint16_t)buf[i], 2);
        fwrite(out, 4, n, w->file);
        w->frames += (uint32_t)n;
        if (n < 2048) break;
    }
}

int rc_wav_close(rc_wav *w) {
    if (!w->file) return 1;
    const int ok = wav_header(w) && !ferror(w->file);
    const int closed = fclose(w->file) == 0;
    w->file = NULL;
    return closed && ok;
}
