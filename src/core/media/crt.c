#include "crt.h"
#include "image.h"
#include <stdlib.h>
#include <string.h>
static uint16_t be16(const uint8_t *p){return (uint16_t)((unsigned)p[0]*256u+p[1]);}
static uint32_t be32(const uint8_t *p){return (uint32_t)p[0]<<24|(uint32_t)p[1]<<16|(uint32_t)p[2]<<8|p[3];}
void rc_crt_init(rc_crt *c){memset(c,0,sizeof *c);}
void rc_crt_close(rc_crt *c){if(c->owned)rc_media_close(c->image);free(c->chips);rc_crt_init(c);}
int rc_crt_attach(rc_crt *c,rc_media *m,char *error,size_t cap){
    if(!m||!m->data||m->size<64||memcmp(m->data,"C64 CARTRIDGE   ",16))return rc_image_error(error,cap,"Not a C64 CRT image");
    const uint8_t *p=m->data;size_t header=be32(p+16),at,count=0;
    /* Historical files declare $20 but still contain the complete $40 header. */
    if(header==32)header=64;
    if(header<64||header>m->size||be16(p+20)!=0x100||p[24]>1||p[25]>1)return rc_image_error(error,cap,"Invalid CRT header");
    for(at=header;at<m->size;){
        if(m->size-at<16||memcmp(p+at,"CHIP",4))return rc_image_error(error,cap,"Invalid CRT CHIP header");
        size_t packet=be32(p+at+4),bytes=be16(p+at+14);unsigned type=be16(p+at+8),address=be16(p+at+12);
        if(packet<16||packet>m->size-at||!bytes||type>2||bytes>65536u-address||
            (type==1?packet!=16&&packet!=16+bytes:packet<16+bytes))return rc_image_error(error,cap,"Invalid CRT CHIP length or type");
        ++count;at+=packet;
    }
    if(!count&&be16(p+22)!=RC_CRT_EASYFLASH)return rc_image_error(error,cap,"CRT has no CHIP packets");
    rc_crt next;rc_crt_init(&next);next.chips=calloc(count?count:1,sizeof *next.chips);
    if(!next.chips)return rc_image_error(error,cap,"Out of memory");
    next.image=m;next.count=count;next.version=be16(p+20);next.hardware=be16(p+22);next.exrom=p[24];next.game=p[25];next.revision=p[26];memcpy(next.name,p+32,32);
    for(at=header,count=0;at<m->size;at+=be32(p+at+4),++count){rc_crt_chip *ch=&next.chips[count];ch->type=be16(p+at+8);ch->bank=be16(p+at+10);ch->address=be16(p+at+12);ch->size=be16(p+at+14);ch->offset=at+16;
        /* RAM CHIP packets without data describe hardware, not file storage. */
        if(ch->type==1&&be32(p+at+4)==16)ch->offset=SIZE_MAX;
        for(size_t i=0;i<count;++i){const rc_crt_chip *a=&next.chips[i];if(a->bank==ch->bank&&a->address<(unsigned)ch->address+ch->size&&ch->address<(unsigned)a->address+a->size){rc_crt_close(&next);return rc_image_error(error,cap,"Overlapping CRT CHIP packets");}}
    }
    /* Reattaching the same object must not free the borrowed media. */
    if(c->image==m&&c->owned){next.owned=1;c->owned=0;}
    rc_crt_close(c);*c=next;return 1;
}
int rc_crt_open(rc_crt *c,const uint8_t *data,size_t size,char *error,size_t cap){
    rc_media *m=rc_media_from("",data,size);if(!m)return rc_image_error(error,cap,"Out of memory");
    if(!rc_crt_attach(c,m,error,cap)){rc_media_close(m);return 0;}c->owned=1;return 1;
}
const rc_crt_chip *rc_crt_find(const rc_crt *c,unsigned bank,unsigned address){
    for(size_t i=0;i<c->count;++i){const rc_crt_chip *ch=&c->chips[i];if(ch->bank==bank&&address>=ch->address&&address<(unsigned)ch->address+ch->size)return ch;}return NULL;
}
int rc_crt_write(rc_crt *c,const rc_crt_chip *ch,unsigned offset,uint8_t value){
    if(!c->image||!c->image->writable||!ch||ch->type==0||offset>=ch->size||ch->offset==SIZE_MAX||ch->offset+offset>=c->image->size)return 0;
    uint8_t *p=c->image->data+ch->offset+offset;if(*p!=value){*p=value;c->image->dirty=1;}return 1;
}
void rc_crt_save(const rc_crt *c,rc_state_writer *w){rc_sw_u32(w,1);rc_sw_u8(w,c->image!=NULL);if(c->image)rc_image_save(c->image,w);}
int rc_crt_load(rc_crt *c,rc_state_reader *r){
    if(rc_sr_u32(r)!=1)return 0;uint8_t present=rc_sr_u8(r);if(!rc_sr_ok(r)||present>1)return 0;
    if(!present){rc_crt_close(c);return 1;}
    rc_media *m=rc_image_load(r);if(!m)return 0;rc_crt next;rc_crt_init(&next);char error[128];
    if(!rc_crt_attach(&next,m,error,sizeof error)){rc_media_close(m);return 0;}next.owned=1;
    if(c->image&&!c->owned){
        rc_media *borrowed=c->image;
        /* Sparse flash images may gain CHIP records after the snapshot. */
        free(borrowed->data);borrowed->data=m->data;borrowed->size=m->size;borrowed->dirty=m->dirty;m->data=NULL;
        rc_media_close(m);next.image=borrowed;next.owned=0;
    }
    rc_crt_close(c);*c=next;return 1;
}
