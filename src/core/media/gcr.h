/* Commodore 1541 GCR blocks and circular raw-bit track buffer.
 * A bit is a recorded flux transition (1) or its absence (0), MSB first.
 * The 1541 ROM, not the VIA, performs block encoding/decoding. */
#ifndef RC_GCR_H
#define RC_GCR_H
#include "image.h"
void rc_gcr_encode4(const uint8_t data[4],uint8_t encoded[5]);
int rc_gcr_decode5(const uint8_t encoded[5],uint8_t data[4]);
void rc_gcr_header(uint8_t track,uint8_t sector,const uint8_t id[2],uint8_t encoded[10]);
int rc_gcr_header_read(const uint8_t encoded[10],uint8_t *track,uint8_t *sector,uint8_t id[2]);
void rc_gcr_data(const uint8_t data[256],uint8_t encoded[325]);
int rc_gcr_data_read(const uint8_t encoded[325],uint8_t data[256]);
/* Zone is the two VIA density-select bits: 3 for tracks 1-17, then 2/1/0.
 * Bit duration is expressed in 16 MHz master-clock ticks, with no rounding. */
unsigned rc_gcr_zone(unsigned track);
unsigned rc_gcr_bit_ticks(unsigned zone);
typedef struct rc_gcr_track {rc_media *image;uint32_t bits,position;uint8_t zone;} rc_gcr_track;
int rc_gcr_track_open(rc_gcr_track *t,const uint8_t *data,size_t bytes,uint32_t bits,unsigned zone);
void rc_gcr_track_close(rc_gcr_track *t);
int rc_gcr_track_read(rc_gcr_track *t);
int rc_gcr_track_write(rc_gcr_track *t,int bit);
void rc_gcr_track_save(const rc_gcr_track *t,rc_state_writer *w);
int rc_gcr_track_load(rc_gcr_track *t,rc_state_reader *r);
#endif
