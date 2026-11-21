#include "gcr.h"
#include <string.h>
static const uint8_t CODES[16]={10,11,18,19,14,15,22,23,9,25,26,27,13,29,30,21};
void rc_gcr_encode4(const uint8_t data[4],uint8_t encoded[5]) {
    uint64_t bits=0;
    for(unsigned i=0;i<4;++i)bits=(bits<<10)|(unsigned)(CODES[data[i]>>4]<<5)|CODES[data[i]&15u];
    for(unsigned i=0;i<5;++i)encoded[i]=(uint8_t)(bits>>(32u-i*8u));
}
/* The first `nibbles` five-bit codes of a group; 0 when one is not a GCR code. */
static int decode_nibbles(const uint8_t encoded[5],uint8_t data[4],unsigned nibbles) {
    uint64_t bits=0;uint8_t decoded[4]={0};
    for(unsigned i=0;i<5;++i)bits=bits<<8|encoded[i];
    for(unsigned n=0;n<nibbles;++n){
        const unsigned code=(unsigned)(bits>>(35u-n*5u))&31u;
        unsigned nibble=0;while(nibble<16&&CODES[nibble]!=code)++nibble;
        if(nibble==16)return 0;
        decoded[n/2]|=(uint8_t)(nibble<<(n&1u?0:4));
    }
    memcpy(data,decoded,4);return 1;
}
int rc_gcr_decode5(const uint8_t encoded[5],uint8_t data[4]){return decode_nibbles(encoded,data,8);}
void rc_gcr_header(uint8_t track,uint8_t sector,const uint8_t id[2],uint8_t encoded[10]) {
    const uint8_t data[8]={8,(uint8_t)(track^sector^id[0]^id[1]),sector,track,id[1],id[0],15,15};
    rc_gcr_encode4(data,encoded);rc_gcr_encode4(data+4,encoded+5);
}
int rc_gcr_header_read(const uint8_t encoded[10],uint8_t *track,uint8_t *sector,uint8_t id[2]) {
    uint8_t data[8];
    if(!rc_gcr_decode5(encoded,data)||!rc_gcr_decode5(encoded+5,data+4)||data[0]!=8||
       data[1]!=(uint8_t)(data[2]^data[3]^data[4]^data[5]))return 0;
    *track=data[3];*sector=data[2];id[0]=data[5];id[1]=data[4];return 1;
}
void rc_gcr_data(const uint8_t data[256],uint8_t encoded[325]) {
    uint8_t block[260]={7},checksum=0;memcpy(block+1,data,256);
    for(unsigned i=0;i<256;++i)checksum^=data[i];block[257]=checksum;
    for(unsigned i=0;i<65;++i)rc_gcr_encode4(block+i*4,encoded+i*5);
}
int rc_gcr_data_read(const uint8_t encoded[325],uint8_t data[256]) {
    uint8_t block[260],checksum=0;
    /* The two bytes after the checksum carry nothing, and a write that ends
     * as the drive switches back to reading may leave the last one cut. */
    for(unsigned i=0;i<65;++i)if(!decode_nibbles(encoded+i*5,block+i*4,i<64?8u:4u))return 0;
    if(block[0]!=7)return 0;for(unsigned i=1;i<257;++i)checksum^=block[i];
    if(checksum!=block[257])return 0;memcpy(data,block+1,256);return 1;
}
unsigned rc_gcr_zone(unsigned track){return track<=17?3:track<=24?2:track<=30?1:0;}
unsigned rc_gcr_bit_ticks(unsigned zone){return zone<=3?(16u-zone)*4u:0;}
int rc_gcr_track_open(rc_gcr_track *t,const uint8_t *data,size_t bytes,uint32_t bits,unsigned zone) {
    if(!bits||bytes!=(bits+7ull)/8||bytes>RC_MEDIA_LIMIT||zone>3)return 0;
    rc_media *image=rc_media_from("",data,bytes);if(!image)return 0;
    memset(t,0,sizeof *t);t->image=image;t->bits=bits;t->zone=(uint8_t)zone;return 1;
}
int rc_gcr_track_speeds(rc_gcr_track *t,const uint8_t *packed){
    const size_t count=(t->image->size+3u)/4u;rc_media *speeds=rc_media_from("",packed,count);if(!speeds)return 0;
    rc_media_close(t->speeds);t->speeds=speeds;return 1;
}
unsigned rc_gcr_track_zone(const rc_gcr_track *t){return t->speeds?(t->speeds->data[t->position/32]>>(6u-2u*((t->position/8)&3u)))&3u:t->zone;}
void rc_gcr_track_write_zone(rc_gcr_track *t,unsigned zone){
    if(t->speeds){const unsigned at=t->position/32,shift=6u-2u*((t->position/8)&3u);t->speeds->data[at]=(uint8_t)((t->speeds->data[at]&~(3u<<shift))|(zone<<shift));}
}
void rc_gcr_track_close(rc_gcr_track *t){rc_media_close(t->image);rc_media_close(t->speeds);memset(t,0,sizeof *t);}
static void advance(rc_gcr_track *t){if(++t->position==t->bits)t->position=0;}
int rc_gcr_track_read(rc_gcr_track *t) {
    if(!t->image||!t->bits)return -1;
    const int bit=t->image->data[t->position/8]>>(7u-t->position%8)&1u;advance(t);return bit;
}
int rc_gcr_track_write(rc_gcr_track *t,int bit) {
    if(!t->image||!t->image->writable||!t->bits)return 0;
    const uint8_t mask=(uint8_t)(128u>>(t->position%8));uint8_t *data=t->image->data+t->position/8;
    *data=(uint8_t)((*data&~mask)|(bit?mask:0));t->image->dirty=1;advance(t);return 1;
}
void rc_gcr_track_save(const rc_gcr_track *t,rc_state_writer *w) {
    rc_sw_u32(w,2);rc_sw_u32(w,t->bits);rc_sw_u32(w,t->position);rc_sw_u8(w,t->zone);rc_image_save(t->image,w);
    rc_sw_u8(w,t->speeds!=NULL);if(t->speeds)rc_image_save(t->speeds,w);
}
int rc_gcr_track_load(rc_gcr_track *t,rc_state_reader *r) {
    if(r->end-r->at<13)return 0;const uint32_t version=rc_sr_u32(r);if(version<1||version>2)return 0;
    const uint32_t bits=rc_sr_u32(r),position=rc_sr_u32(r);const uint8_t zone=rc_sr_u8(r);
    rc_media *image=rc_image_load(r);if(!image)return 0;
    if(!bits||position>=bits||zone>3||image->size!=(bits+7ull)/8){rc_media_close(image);return 0;}
    rc_media *speeds=NULL;if(version==2){const unsigned present=rc_sr_u8(r);if(present>1){rc_media_close(image);return 0;}
        if(present){speeds=rc_image_load(r);if(!speeds||speeds->size!=(image->size+3u)/4u){rc_media_close(image);rc_media_close(speeds);return 0;}}}
    if(!rc_sr_ok(r)){rc_media_close(image);rc_media_close(speeds);return 0;}
    rc_gcr_track_close(t);t->image=image;t->speeds=speeds;t->bits=bits;t->position=position;t->zone=zone;return 1;
}
