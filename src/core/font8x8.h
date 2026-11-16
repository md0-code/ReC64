/* Original Commodore-style 8x8 glyphs and shell drawing marks. */
#ifndef RC_FONT8X8_H
#define RC_FONT8X8_H

#include <stdint.h>

/* The glyph codes of the marks and pieces the shell draws with. */
enum {
    RC_GLYPH_UP = 0x5E, RC_GLYPH_POUND = 0x60, RC_GLYPH_COPYRIGHT = 0x7F,
    RC_GLYPH_BLOCK = 0x80,              /* + the quarters wanted (1 top right, 2 top left, 4 bottom right, 8 bottom left) */
    RC_GLYPH_FULL = 0x8F,
    RC_GLYPH_DOWN = 0x9A, RC_GLYPH_POINT_RIGHT = 0x9B, RC_GLYPH_POINT_LEFT = 0x9C, RC_GLYPH_ELLIPSIS = 0x9D,
    RC_GLYPH_BULLET = 0x9E, RC_GLYPH_STRIPE = 0x9F,
    RC_GLYPH_LINE_H = 0xA0, RC_GLYPH_LINE_V = 0xA1,
    RC_GLYPH_CORNER_TL = 0xA2, RC_GLYPH_CORNER_TR = 0xA3, RC_GLYPH_CORNER_BL = 0xA4, RC_GLYPH_CORNER_BR = 0xA5,
    RC_GLYPH_TEE_L = 0xA6, RC_GLYPH_TEE_R = 0xA7, RC_GLYPH_TEE_DOWN = 0xA8, RC_GLYPH_TEE_UP = 0xA9, RC_GLYPH_CROSS = 0xAA,
    RC_GLYPH_SHADE = 0xAB
};

/* A glyph code as a char (the codes above 7Fh do not fit a signed one: the value is the code less 256). */
#define RC_GLYPH_CHAR(code) ((char)((code) > 127 ? (code) - 256 : (code)))

/* 20h-7Fh. */
extern const uint8_t rc_font8x8_ascii[96][8];

void rc_font8x8_build(uint8_t font[2048]);

#endif /* RC_FONT8X8_H */
