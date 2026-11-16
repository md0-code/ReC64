#include "prg.h"
#include <stdlib.h>
#include <string.h>
int rc_prg_open(rc_prg *p,const uint8_t *data,size_t size,int p00,char *error,size_t cap){
    const size_t offset=p00?26u:0u;memset(p,0,sizeof *p);
    if(size<offset+2u||size>offset+65538u||(p00&&(memcmp(data,"C64File\0",8)||data[24]||data[25])))return rc_image_error(error,cap,"Invalid PRG/P00 header or size");
    const uint16_t address=rc_image_u16(data+offset);const size_t length=size-offset-2u;
    if(length>65536u-address)return rc_image_error(error,cap,"Program extends beyond C64 memory");
    p->image=rc_media_from("",data,size);if(!p->image)return rc_image_error(error,cap,"Out of memory");p->p00=p00!=0;p->address=address;p->length=(uint32_t)length;if(p00)memcpy(p->name,data+8,16);return 1;
}
int rc_prg_create(rc_prg *p,uint16_t address,const uint8_t *data,size_t size,const uint8_t name[16],int p00,char *error,size_t cap){
    if(size>65536u-address)return rc_image_error(error,cap,"Program extends beyond C64 memory");
    const size_t offset=p00?26u:0u;uint8_t *bytes=calloc(offset+2+size,1);if(!bytes)return rc_image_error(error,cap,"Out of memory");
    if(p00){memcpy(bytes,"C64File",7);if(name)memcpy(bytes+8,name,16);}rc_image_put16(bytes+offset,address);if(size)memcpy(bytes+offset+2,data,size);
    const int ok=rc_prg_open(p,bytes,offset+2+size,p00,error,cap);free(bytes);if(ok)p->image->writable=1;return ok;
}
const uint8_t *rc_prg_data(const rc_prg *p){return p->image?p->image->data+(p->p00?28:2):NULL;}
void rc_prg_close(rc_prg *p){rc_media_close(p->image);memset(p,0,sizeof *p);}
void rc_prg_save(const rc_prg *p,rc_state_writer *w){rc_sw_u32(w,1);rc_sw_u8(w,(uint8_t)p->p00);rc_image_save(p->image,w);}
int rc_prg_load(rc_prg *p,rc_state_reader *r){if(r->end-r->at<5||rc_sr_u32(r)!=1)return 0;const unsigned p00=rc_sr_u8(r);rc_media *m=rc_image_load(r);if(!m||p00>1){rc_media_close(m);return 0;}rc_prg next;char error[128];const int ok=rc_prg_open(&next,m->data,m->size,(int)p00,error,sizeof error);if(ok){rc_media_close(next.image);next.image=m;rc_prg_close(p);*p=next;}else rc_media_close(m);return ok;}
