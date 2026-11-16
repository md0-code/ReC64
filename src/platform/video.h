
/* The frame presenter.
 *
 * Frames arrive as ARGB8888 of any size and are shown at the configured
 * display mode, scale, shape and filtering, through OpenGL 3.3 core or,
 * when that cannot be had, OpenGL ES 3.0 (plain or the CRT shader, one
 * shader source for both), and when neither can, through the SDL renderer
 * without the shader. Every option can change while ReC64 runs. Nothing
 * here knows what is in the picture. */

#ifndef RC_VIDEO_H
#define RC_VIDEO_H

#include "settings.h"

#include <SDL.h>
#include <stdbool.h>
#include <stdint.h>

typedef struct rc_video rc_video;

/* `screen_w` x `screen_h` is the guest screen's resolution and `aspect_w:aspect_h`
 * the shape it had on a monitor; both fix the layout whatever the size of
 * the frames shown. `hidden` opens no visible window (headless runs).
 * The environment variable REC64_VIDEO=gles or =renderer starts the chain
 * at that step (checks of the fallbacks). */
rc_video *rc_video_open(const char *title, const rc_settings *s, int screen_w, int screen_h, int aspect_w,
                        int aspect_h, bool hidden, char *err, size_t errlen);
void rc_video_close(rc_video *v);
/* What presents: "OpenGL 3.3 core", "OpenGL ES 3.0" or "SDL renderer". */
const char *rc_video_backend(const rc_video *v);

void rc_video_present(rc_video *v, const uint32_t *frame, int w, int h);
/* The guest screen's resolution and its shape on the monitor. */
void rc_video_set_geometry(rc_video *v, int w, int h, int aspect_w, int aspect_h);

/* Live changes; the settings stay the caller's to save. */
/* Full screen is always borderless, on the desktop at its own resolution:
 * no mode switch, instant toggling, and the only kind every platform has. */
void rc_video_set_fullscreen(rc_video *v, bool on);
/* The window's icon: `size` x `size` ARGB pixels (copied). */
void rc_video_set_icon(rc_video *v, const uint32_t *argb, int size);
bool rc_video_fullscreen(const rc_video *v);
void rc_video_set_filter(rc_video *v, int filter);
void rc_video_set_integer_scale(rc_video *v, int scale);
void rc_video_set_aspect(rc_video *v, int aspect);
void rc_video_set_vsync(rc_video *v, bool on);
bool rc_video_crt_available(const rc_video *v);
/* How the picture is shown: plain; a CRT (the CRT shader's preset: curvature,
 * scanlines, aperture stripe, glow); or an LCD (sharp, flat, a faint grid
 * between pixels shown two or more output pixels wide); in full colour or
 * in one phosphor colour (a monochrome monitor: the picture's brightness
 * in white, gray, green or amber). All but plain in colour need the shader:
 * false when it is unavailable, and the picture stays plain. */
enum { RC_VIDEO_LOOK_PLAIN, RC_VIDEO_LOOK_CRT, RC_VIDEO_LOOK_LCD };
enum { RC_VIDEO_PHOSPHOR_COLOR, RC_VIDEO_PHOSPHOR_WHITE, RC_VIDEO_PHOSPHOR_GRAY, RC_VIDEO_PHOSPHOR_GREEN,
       RC_VIDEO_PHOSPHOR_AMBER };
bool rc_video_set_look(rc_video *v, int look, int phosphor);

/* A window position (mouse event coordinates) in screen pixels; false when it
 * is outside the picture (the result is clamped to the picture). */
bool rc_video_to_screen(const rc_video *v, int wx, int wy, int *gx, int *gy);

/* Save the next presented frame as a PNG when the name ends in .png, else as
 * a BMP: as it appears in the window, or `raw` as the frame itself at its
 * own size. */
void rc_video_request_shot(rc_video *v, const char *path, bool raw);
/* The path the last shot went to, empty when it failed; cleared by reading. */
bool rc_video_take_shot_result(rc_video *v, char *path, size_t len);

#endif /* RC_VIDEO_H */
