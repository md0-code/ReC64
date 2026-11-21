#include "t64.h"
#include <stdlib.h>
#include <string.h>
static int active(const uint8_t *p){return p[0]==1&&p[1]!=0;}
int rc_t64_entry_at(const rc_t64 *t,unsigned entry,rc_t64_entry *out){
    if(entry>=t->count)return 0;const uint8_t *data=t->image->data;unsigned slot=0;
    for(;slot<t->capacity;++slot){const uint8_t *p=data+64+32u*slot;if(active(p)){if(!entry)break;--entry;}}
    if(slot==t->capacity)return 0;const uint8_t *p=data+64+32u*slot;const uint32_t off=rc_image_u32(p+8);size_t end=t->image->size;
    for(unsigned i=0;i<t->capacity;++i){const uint8_t *q=data+64+32u*i;const uint32_t pos=rc_image_u32(q+8);if(active(q)&&pos>off&&pos<end)end=pos;}
    const uint16_t start=rc_image_u16(p+2),stop=rc_image_u16(p+4);const uint32_t declared=stop?stop:(start||end>off?65536u:0u);
    if(off<64+32u*t->capacity||off>end)return 0;size_t length;
    /* An entry whose end address runs past its data, often the last of a cut file, loads what is there. */
    if(stop==0xc3c6)length=end-off;else {if(declared<start)return 0;length=declared-start;if(length>end-off)length=end-off;}
    if(length>65536u-start)return 0;out->address=start;out->slot=(uint16_t)slot;out->offset=off;out->length=(uint32_t)length;out->type=p[1];memcpy(out->name,p+16,16);return 1;
}
int rc_t64_open(rc_t64 *t,const uint8_t *data,size_t size,char *error,size_t cap){memset(t,0,sizeof *t);if(size<64||size>RC_MEDIA_LIMIT||memcmp(data,"C64",3)||(rc_image_u16(data+32)!=0x100&&rc_image_u16(data+32)!=0x101))return rc_image_error(error,cap,"Invalid T64 header");t->capacity=rc_image_u16(data+34);if(64+32u*t->capacity>size||rc_image_u16(data+36)>t->capacity)return rc_image_error(error,cap,"T64 directory is truncated");for(unsigned i=0;i<t->capacity;++i)if(active(data+64+32u*i))++t->count;t->image=rc_media_from("",data,size);if(!t->image)return rc_image_error(error,cap,"Out of memory");for(unsigned i=0;i<t->count;++i){rc_t64_entry e;if(!rc_t64_entry_at(t,i,&e)){rc_t64_close(t);return rc_image_error(error,cap,"T64 program is outside its data area");}}return 1;}
int rc_t64_create(rc_t64 *t,const rc_t64_program *programs,unsigned count,const uint8_t name[24],char *error,size_t cap){if(count>65535)return rc_image_error(error,cap,"Too many T64 entries");size_t size=64+32u*count;for(unsigned i=0;i<count;++i){if(programs[i].size>65536u-programs[i].address||programs[i].size>RC_MEDIA_LIMIT-size)return rc_image_error(error,cap,"T64 program exceeds its address range");size+=programs[i].size;}uint8_t *bytes=calloc(size,1);if(!bytes)return rc_image_error(error,cap,"Out of memory");memcpy(bytes,"C64S tape image file",19);rc_image_put16(bytes+32,0x101);rc_image_put16(bytes+34,(uint16_t)count);rc_image_put16(bytes+36,(uint16_t)count);memset(bytes+40,32,24);if(name)memcpy(bytes+40,name,24);size_t off=64+32u*count;for(unsigned i=0;i<count;++i){uint8_t *p=bytes+64+32u*i;p[0]=1;p[1]=0x82;rc_image_put16(p+2,programs[i].address);rc_image_put16(p+4,(uint16_t)(programs[i].address+programs[i].size));rc_image_put32(p+8,(uint32_t)off);memcpy(p+16,programs[i].name,16);if(programs[i].size)memcpy(bytes+off,programs[i].data,programs[i].size);off+=programs[i].size;}const int ok=rc_t64_open(t,bytes,size,error,cap);free(bytes);if(ok)t->image->writable=1;return ok;}
void rc_t64_close(rc_t64 *t){rc_media_close(t->image);memset(t,0,sizeof *t);}
void rc_t64_save(const rc_t64 *t,rc_state_writer *w){rc_sw_u32(w,1);rc_image_save(t->image,w);}
int rc_t64_load(rc_t64 *t,rc_state_reader *r){if(r->end-r->at<4||rc_sr_u32(r)!=1)return 0;rc_media *m=rc_image_load(r);if(!m)return 0;rc_t64 next;char error[128];if(!rc_t64_open(&next,m->data,m->size,error,sizeof error)){rc_media_close(m);return 0;}rc_media_close(next.image);next.image=m;rc_t64_close(t);*t=next;return 1;}
