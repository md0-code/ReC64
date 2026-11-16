/* Presenter settings filled by the host shell. */

#ifndef RC_SETTINGS_H
#define RC_SETTINGS_H

#include <stdbool.h>

#define RC_PATH_MAX 1024

typedef enum rc_filter { RC_FILTER_NEAREST, RC_FILTER_LINEAR, RC_FILTER_SHARP } rc_filter;
typedef enum rc_aspect { RC_ASPECT_ORIGINAL, RC_ASPECT_SQUARE } rc_aspect;

typedef struct rc_settings {
    bool fullscreen;            /* borderless, on the desktop */
    int window_scale;           /* the window at start: the picture times this */
    int integer_scale;          /* 0: fill the window; N: whole multiples up to N */
    int aspect;                 /* rc_aspect */
    int filter;                 /* rc_filter */
    bool crt;
    bool vsync;
} rc_settings;

#endif /* RC_SETTINGS_H */
