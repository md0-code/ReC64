#include "c64kbd.h"
#include <stdio.h>
#define CHECK(x) do{if(!(x)){fprintf(stderr,"line %d: %s\n",__LINE__,#x);return 1;}}while(0)
int main(void){rc_c64kbd k;rc_c64kbd_init(&k,NULL);rc_c64kbd_key(&k,10,1);uint8_t a=0xfd,b=0xff;rc_c64kbd_scan(&k,&a,&b);CHECK(b==0xfb);
    rc_c64kbd_key(&k,11,1);rc_c64kbd_key(&k,18,1);a=0xff;b=0xf7;rc_c64kbd_scan(&k,&a,&b);CHECK(a==0xf9&&b==0xf3);
    rc_state_writer w;rc_sw_init(&w);rc_sw_header(&w,"test","keyboard",NULL,0);rc_sw_begin(&w,"KEYS",0);rc_c64kbd_save(&k,&w);rc_sw_end(&w);CHECK(rc_sw_finish(&w));rc_c64kbd_key(&k,10,0);
    rc_state_reader r;char error[128];CHECK(rc_sr_open(&r,w.data,w.size,error,sizeof error)&&rc_sr_chunk(&r,"KEYS",0)&&rc_c64kbd_load(&k,&r));a=0xfd;b=0xff;rc_c64kbd_scan(&k,&a,&b);CHECK(b==0xf3);rc_sw_free(&w);puts("Keyboard matrix, ghosting and state OK");return 0;}
