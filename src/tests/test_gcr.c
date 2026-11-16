#include "media/gcr.h"
#include <stdio.h>
#include <string.h>
#define CHECK(x) do{if(!(x)){fprintf(stderr,"line %d: %s\n",__LINE__,#x);return 1;}}while(0)
int main(void) {
    const uint8_t raw[4]={8,0x10,0,0x12},gold[5]={0x52,0x56,0xa5,0x29,0x72};
    uint8_t encoded[325],decoded[256],payload[256],header[10],track,sector,id[2],disk_id[2]={0x41,0x42};
    rc_gcr_encode4(raw,encoded);CHECK(!memcmp(encoded,gold,5));CHECK(rc_gcr_decode5(encoded,decoded)&&!memcmp(decoded,raw,4));
    memset(encoded,0,5);CHECK(!rc_gcr_decode5(encoded,decoded));
    rc_gcr_header(18,7,disk_id,header);CHECK(rc_gcr_header_read(header,&track,&sector,id)&&track==18&&sector==7&&!memcmp(id,disk_id,2));
    for(unsigned i=0;i<256;++i)payload[i]=(uint8_t)i;
    rc_gcr_data(payload,encoded);CHECK(rc_gcr_data_read(encoded,decoded)&&!memcmp(decoded,payload,256));
    uint8_t tail[4];CHECK(rc_gcr_decode5(encoded+320,tail));tail[1]^=1;rc_gcr_encode4(tail,encoded+320);CHECK(!rc_gcr_data_read(encoded,decoded));
    CHECK(rc_gcr_zone(1)==3&&rc_gcr_zone(18)==2&&rc_gcr_zone(25)==1&&rc_gcr_zone(31)==0&&rc_gcr_bit_ticks(3)==52&&rc_gcr_bit_ticks(0)==64);
    rc_gcr_track t={0};CHECK(rc_gcr_track_open(&t,gold,5,39,3));
    t.image->writable=1;for(unsigned i=0;i<11;++i)CHECK(rc_gcr_track_read(&t)==(int)(gold[i/8]>>(7u-i%8)&1u));
    CHECK(rc_gcr_track_write(&t,1));
    rc_state_writer w;rc_sw_init(&w);rc_sw_header(&w,"test","gcr",NULL,0);rc_sw_begin(&w,"GCR ",0);rc_gcr_track_save(&t,&w);rc_sw_end(&w);CHECK(rc_sw_finish(&w));
    uint8_t bits[41];for(unsigned i=0;i<41;++i)bits[i]=(uint8_t)rc_gcr_track_read(&t);const unsigned position=t.position;
    rc_state_reader r;char error[128];CHECK(rc_sr_open(&r,w.data,w.size,error,sizeof error)&&rc_sr_chunk(&r,"GCR ",0)&&rc_gcr_track_load(&t,&r));
    for(unsigned i=0;i<41;++i)CHECK(rc_gcr_track_read(&t)==bits[i]);CHECK(t.position==position&&t.zone==3&&t.image->dirty);
    rc_gcr_track_close(&t);rc_sw_free(&w);puts("GCR published vector, blocks, bit stream and state OK");return 0;
}
