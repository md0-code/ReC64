/* Peter Schepers' G64 v0 container. Descriptors borrow the mounted image. */
#ifndef RC_G64_H
#define RC_G64_H
#include "image.h"
#define RC_G64_TRACKS 84u
typedef struct rc_g64 {
    rc_media *image;
    uint32_t offset[RC_G64_TRACKS],speed[RC_G64_TRACKS];
    uint16_t length[RC_G64_TRACKS],maximum;
    unsigned tracks;
} rc_g64;
int rc_g64_open(rc_g64 *g,rc_media *image,char *error,size_t cap);
unsigned rc_g64_speed(const rc_g64 *g,unsigned halftrack,unsigned byte);
/* Index zero is track 1, index one track 1.5. Packed speeds are MSB first. */
int rc_g64_write(rc_g64 *g,unsigned halftrack,const uint8_t *bytes,unsigned length,const uint8_t *speeds,unsigned zone);
#endif
