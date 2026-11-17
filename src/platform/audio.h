/* Host sound output: the machine's samples, pulled after each run slice,
 * converted to the output device's rate with a ratio that follows the
 * device's own clock (audiosync.h: the queue neither grows nor runs dry) and
 * queued at the volume asked for. */
#ifndef RC_HOST_AUDIO_H
#define RC_HOST_AUDIO_H

#include "../core/machine.h"
#include "audiosync.h"

#include <SDL.h>
#include <stdio.h>

typedef struct rc_host_audio {
    SDL_AudioDeviceID device;
    SDL_AudioSpec output;
    rc_audiosync sync;
    uint32_t rate;              /* the machine's rate the sync was set up for */
    int flowing;                /* samples have been queued since the last discard */
} rc_host_audio;

/* 0 when no output device opens; pumping then discards the samples. */
int rc_host_audio_open(rc_host_audio *audio);
/* Moves the machine's samples to the device at `volume` percent (0 mutes);
 * with `discard` (the machine runs faster than real time, or is not
 * running) they are taken and dropped; with `racing` (the machine runs ahead
 * of the clock with its sound on) only what keeps the queue at its usual
 * length is played. 0 on an SDL failure. */
int rc_host_audio_pump(rc_host_audio *audio, rc_machine *machine, unsigned volume, int discard, int racing);
void rc_host_audio_close(rc_host_audio *audio);

/* The machine's sound recorded to a WAV file instead (16-bit stereo PCM at
 * the machine's rate): rc_wav_pump after each run slice. */
typedef struct rc_wav {
    FILE *file;
    uint32_t rate, frames;
    int peak;                   /* loudest sample so far */
} rc_wav;
int rc_wav_open(rc_wav *wav, const char *path);
void rc_wav_pump(rc_wav *wav, rc_machine *machine);
/* Completes the header; 0 when the file could not be written. */
int rc_wav_close(rc_wav *wav);

#endif /* RC_HOST_AUDIO_H */
