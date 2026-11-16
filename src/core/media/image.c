#include "image.h"
#include <stdlib.h>
void rc_image_save(const rc_media *m,rc_state_writer *w){if(!m){w->failed=1;return;}rc_sw_str(w,m->path);rc_sw_u8(w,(uint8_t)m->writable);rc_sw_u8(w,(uint8_t)m->dirty);rc_sw_packed(w,m->data,m->size);}
rc_media *rc_image_load(rc_state_reader *r){
    char path[RC_MEDIA_PATH];rc_sr_str(r,path,sizeof path);const uint8_t writable=rc_sr_u8(r),dirty=rc_sr_u8(r);
    if(!rc_sr_ok(r)||r->end-r->at<4||writable>1||dirty>1)return NULL;
    const uint32_t size=rc_image_u32(r->data+r->at);if(size>RC_MEDIA_LIMIT)return NULL;
    uint8_t *bytes=malloc(size?size:1);if(!bytes)return NULL;rc_sr_packed(r,bytes,size);
    rc_media *m=rc_sr_ok(r)?rc_media_from(path,bytes,size):NULL;free(bytes);if(m){m->writable=writable;m->dirty=dirty;}return m;
}
