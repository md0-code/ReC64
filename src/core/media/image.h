#ifndef RC_IMAGE_H
#define RC_IMAGE_H
#include "../media.h"
#include "../state.h"
#include <stdio.h>
static inline uint16_t rc_image_u16(const uint8_t *p){return (uint16_t)(p[0]|p[1]<<8);}
static inline uint32_t rc_image_u32(const uint8_t *p){return p[0]|(uint32_t)p[1]<<8|(uint32_t)p[2]<<16|(uint32_t)p[3]<<24;}
static inline void rc_image_put16(uint8_t *p,uint16_t v){p[0]=(uint8_t)v;p[1]=(uint8_t)(v>>8);}
static inline void rc_image_put32(uint8_t *p,uint32_t v){for(unsigned i=0;i<4;++i)p[i]=(uint8_t)(v>>(i*8));}
static inline int rc_image_error(char *out,size_t cap,const char *text){if(cap)snprintf(out,cap,"%s",text);return 0;}
void rc_image_save(const rc_media *m,rc_state_writer *w);
rc_media *rc_image_load(rc_state_reader *r);
#endif
