#include "g64.h"
#include <string.h>

int rc_g64_open(rc_g64 *out,rc_media *m,char *error,size_t cap){
    rc_g64 g={0};
    if(!m||m->size<12||m->size>RC_MEDIA_LIMIT||memcmp(m->data,"GCR-1541",8))return rc_image_error(error,cap,"Not a G64 image");
    if(m->data[8])return rc_image_error(error,cap,"Unsupported G64 version");
    g.tracks=m->data[9];g.maximum=rc_image_u16(m->data+10);g.image=m;
    const unsigned header=12+8*g.tracks;
    if(!g.tracks||g.tracks>RC_G64_TRACKS||!g.maximum||m->size<header)return rc_image_error(error,cap,"Invalid G64 track table");
    for(unsigned i=0;i<g.tracks;++i){
        const uint32_t at=rc_image_u32(m->data+12+4*i),speed=rc_image_u32(m->data+12+4*g.tracks+4*i);
        g.offset[i]=at;g.speed[i]=speed;
        if(!at)continue;
        if(at<header||at>m->size||m->size-at<2)return rc_image_error(error,cap,"Invalid G64 track offset");
        const unsigned length=rc_image_u16(m->data+at);g.length[i]=(uint16_t)length;
        if(!length||length>g.maximum||length>m->size-at-2)return rc_image_error(error,cap,"Truncated G64 track");
        if(speed>3&&(speed<header||speed>m->size||(length+3u)/4u>m->size-speed))return rc_image_error(error,cap,"Truncated G64 speed map");
    }
    *out=g;return 1;
}
unsigned rc_g64_speed(const rc_g64 *g,unsigned track,unsigned byte){
    if(track>=g->tracks||byte>=g->length[track])return 0;
    const uint32_t speed=g->speed[track];return speed<4?speed:(g->image->data[speed+byte/4]>>(6u-2u*(byte&3u)))&3u;
}
static int extend_table(rc_g64 *g){
    if(g->tracks==RC_G64_TRACKS)return 1;
    rc_media *m=g->image;const size_t before=m->size;const unsigned old_header=12+8*g->tracks,delta=8*(RC_G64_TRACKS-g->tracks);
    if(before>RC_MEDIA_LIMIT-delta||!rc_media_resize(m,before+delta))return 0;
    memmove(m->data+old_header+delta,m->data+old_header,before-old_header);
    memset(m->data+12,0,RC_G64_TRACKS*8);m->data[9]=RC_G64_TRACKS;
    for(unsigned i=0;i<g->tracks;++i){
        if(g->offset[i])g->offset[i]+=delta;
        if(g->speed[i]>3)g->speed[i]+=delta;
        rc_image_put32(m->data+12+4*i,g->offset[i]);rc_image_put32(m->data+12+4*RC_G64_TRACKS+4*i,g->speed[i]);
    }
    g->tracks=RC_G64_TRACKS;m->dirty=1;return 1;
}
static int overlap(uint32_t a,unsigned n,uint32_t b,unsigned m){return a<b+m&&b<a+n;}
static int unique_block(const rc_g64 *g,unsigned track,uint32_t at,unsigned bytes,int map){
    for(unsigned i=0;i<g->tracks;++i)if(g->offset[i]){
        if((i!=track||map)&&overlap(at,bytes,g->offset[i],g->length[i]+2u))return 0;
        if(g->speed[i]>3&&(i!=track||!map)&&overlap(at,bytes,g->speed[i],(g->length[i]+3u)/4u))return 0;
    }return 1;
}
int rc_g64_write(rc_g64 *g,unsigned track,const uint8_t *bytes,unsigned length,const uint8_t *speeds,unsigned zone){
    rc_media *m=g->image;if(!m||!m->writable||track>=RC_G64_TRACKS||!length||length>65535||zone>3)return 0;
    if(track>=g->tracks&&!extend_table(g))return 0;
    unsigned constant=zone;if(speeds){constant=(speeds[0]>>6)&3u;for(unsigned i=1;i<length;++i)if(((speeds[i/4]>>(6u-2u*(i&3u)))&3u)!=constant){constant=4;break;}}
    uint32_t offset=g->offset[track],speed=g->speed[track];
    const unsigned maximum=length>g->maximum?length:g->maximum;
    const unsigned add_track=!offset||g->length[track]!=length||!unique_block(g,track,offset,length+2u,0);
    const unsigned add_map=constant==4&&(speed<4||g->length[track]<length||!unique_block(g,track,speed,(length+3u)/4u,1));
    /* Shared blocks are copied on write; private blocks can be reused. */
    const size_t add=(add_track?maximum+2u:0u)+(add_map?(maximum+3u)/4u:0u),before=m->size;
    if(before>RC_MEDIA_LIMIT-add||(add&&!rc_media_resize(m,before+add)))return 0;
    size_t at=before;if(add_track){offset=(uint32_t)at;at+=maximum+2u;rc_image_put16(m->data+offset,(uint16_t)length);}
    memcpy(m->data+offset+2,bytes,length);
    if(constant==4){if(add_map){speed=(uint32_t)at;memset(m->data+at,0,(maximum+3u)/4u);}memcpy(m->data+speed,speeds,(length+3u)/4u);}else speed=constant;
    g->offset[track]=offset;g->speed[track]=speed;g->length[track]=(uint16_t)length;g->maximum=(uint16_t)maximum;
    rc_image_put16(m->data+10,(uint16_t)maximum);rc_image_put32(m->data+12+4*track,offset);rc_image_put32(m->data+12+4*g->tracks+4*track,speed);m->dirty=1;return 1;
}
