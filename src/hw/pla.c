/* C64 PLA logic equations: Marko Makela/Mark Smith, verified 82S100 fuse
 * map; also published by Thomas Giesel in The C64 PLA Dissected. */
#include "pla.h"
uint8_t rc_pla_decode(uint16_t inputs) {
    const int _CAS=(inputs&(1u<<0))!=0;
    const int _LORAM=(inputs&(1u<<1))!=0;
    const int _HIRAM=(inputs&(1u<<2))!=0;
    const int _CHAREN=(inputs&(1u<<3))!=0;
    const int _VA14=(inputs&(1u<<4))!=0;
    const int A15=(inputs&(1u<<5))!=0;
    const int A14=(inputs&(1u<<6))!=0;
    const int A13=(inputs&(1u<<7))!=0;
    const int A12=(inputs&(1u<<8))!=0;
    const int BA=(inputs&(1u<<9))!=0;
    const int _AEC=(inputs&(1u<<10))!=0;
    const int R__W=(inputs&(1u<<11))!=0;
    const int _EXROM=(inputs&(1u<<12))!=0;
    const int _GAME=(inputs&(1u<<13))!=0;
    const int VA13=(inputs&(1u<<14))!=0;
    const int VA12=(inputs&(1u<<15))!=0;
    uint8_t selected=0;
    if((_HIRAM && A15 && !A14 && A13 && !_AEC && R__W && !_EXROM && !_GAME 
             || A15 && A14 && A13 && !_AEC && _EXROM && !_GAME 
             || _AEC && _EXROM && !_GAME && VA13 && VA12 )) selected|=RC_PLA_ROMH;
    if((_LORAM && _HIRAM && A15 && !A14 && !A13 && !_AEC && R__W && !_EXROM 
             || A15 && !A14 && !A13 && !_AEC && _EXROM && !_GAME )) selected|=RC_PLA_ROML;
    if((_HIRAM && _CHAREN && A15 && A14 && !A13 && A12 && BA && !_AEC && R__W && _GAME 
             || _HIRAM && _CHAREN && A15 && A14 && !A13 && A12 && !_AEC && !R__W && _GAME 
             || _LORAM && _CHAREN && A15 && A14 && !A13 && A12 && BA && !_AEC && R__W && _GAME 
             || _LORAM && _CHAREN && A15 && A14 && !A13 && A12 && !_AEC && !R__W && _GAME 
             || _HIRAM && _CHAREN && A15 && A14 && !A13 && A12 && BA && !_AEC && R__W && !_EXROM && !_GAME 
             || _HIRAM && _CHAREN && A15 && A14 && !A13 && A12 && !_AEC && !R__W && !_EXROM && !_GAME 
             || _LORAM && _CHAREN && A15 && A14 && !A13 && A12 && BA && !_AEC && R__W && !_EXROM && !_GAME 
             || _LORAM && _CHAREN && A15 && A14 && !A13 && A12 && !_AEC && !R__W && !_EXROM && !_GAME 
             || A15 && A14 && !A13 && A12 && BA && !_AEC && R__W && _EXROM && !_GAME 
             || A15 && A14 && !A13 && A12 && !_AEC && !R__W && _EXROM && !_GAME )) selected|=RC_PLA_I_O;
    if((!_CAS && A15 && A14 && !A13 && A12 && !_AEC && !R__W )) selected|=RC_PLA_GR_W;
    if((_HIRAM && !_CHAREN && A15 && A14 && !A13 && A12 && !_AEC && R__W && _GAME 
             || _LORAM && !_CHAREN && A15 && A14 && !A13 && A12 && !_AEC && R__W && _GAME 
             || _HIRAM && !_CHAREN && A15 && A14 && !A13 && A12 && !_AEC && R__W && !_EXROM && !_GAME 
             || _VA14 && _AEC && _GAME && !VA13 && VA12 
             || _VA14 && _AEC && !_EXROM && !_GAME && !VA13 && VA12 )) selected|=RC_PLA_CHAROM;
    if((_HIRAM && A15 && A14 && A13 && !_AEC && R__W && _GAME 
             || _HIRAM && A15 && A14 && A13 && !_AEC && R__W && !_EXROM && !_GAME )) selected|=RC_PLA_KERNAL;
    if((_LORAM && _HIRAM && A15 && !A14 && A13 && !_AEC && R__W && _GAME )) selected|=RC_PLA_BASIC;
    if(!(_LORAM && _HIRAM && A15 && !A14 && A13 && !_AEC && R__W && _GAME 
             || _HIRAM && A15 && A14 && A13 && !_AEC && R__W && _GAME 
             || _HIRAM && A15 && A14 && A13 && !_AEC && R__W && !_EXROM && !_GAME 
             || _HIRAM && !_CHAREN && A15 && A14 && !A13 && A12 && !_AEC && R__W && _GAME 
             || _LORAM && !_CHAREN && A15 && A14 && !A13 && A12 && !_AEC && R__W && _GAME 
             || _HIRAM && !_CHAREN && A15 && A14 && !A13 && A12 && !_AEC && R__W && !_EXROM && !_GAME 
             || _VA14 && _AEC && _GAME && !VA13 && VA12 
             || _VA14 && _AEC && !_EXROM && !_GAME && !VA13 && VA12 
             || _HIRAM && _CHAREN && A15 && A14 && !A13 && A12 && BA && !_AEC && R__W && _GAME 
             || _HIRAM && _CHAREN && A15 && A14 && !A13 && A12 && !_AEC && !R__W && _GAME 
             || _LORAM && _CHAREN && A15 && A14 && !A13 && A12 && BA && !_AEC && R__W && _GAME 
             || _LORAM && _CHAREN && A15 && A14 && !A13 && A12 && !_AEC && !R__W && _GAME 
             || _HIRAM && _CHAREN && A15 && A14 && !A13 && A12 && BA && !_AEC && R__W && !_EXROM && !_GAME 
             || _HIRAM && _CHAREN && A15 && A14 && !A13 && A12 && !_AEC && !R__W && !_EXROM && !_GAME 
             || _LORAM && _CHAREN && A15 && A14 && !A13 && A12 && BA && !_AEC && R__W && !_EXROM && !_GAME 
             || _LORAM && _CHAREN && A15 && A14 && !A13 && A12 && !_AEC && !R__W && !_EXROM && !_GAME 
             || A15 && A14 && !A13 && A12 && BA && !_AEC && R__W && _EXROM && !_GAME 
             || A15 && A14 && !A13 && A12 && !_AEC && !R__W && _EXROM && !_GAME 
             || _LORAM && _HIRAM && A15 && !A14 && !A13 && !_AEC && R__W && !_EXROM 
             || A15 && !A14 && !A13 && !_AEC && _EXROM && !_GAME 
             || _HIRAM && A15 && !A14 && A13 && !_AEC && R__W && !_EXROM && !_GAME 
             || A15 && A14 && A13 && !_AEC && _EXROM && !_GAME 
             || _AEC && _EXROM && !_GAME && VA13 && VA12 
             || !A15 && !A14 && A12 && _EXROM && !_GAME 
             || !A15 && !A14 && A13 && _EXROM && !_GAME 
             || !A15 && A14 && _EXROM && !_GAME 
             || A15 && !A14 && A13 && _EXROM && !_GAME 
             || A15 && A14 && !A13 && !A12 && _EXROM && !_GAME 
             || _CAS )) selected|=RC_PLA_CASRAM;
    return selected;
}
void rc_pla_save(const rc_pla *p,rc_state_writer *w){rc_sw_u32(w,1);rc_sw_u16(w,p->inputs);}
int rc_pla_load(rc_pla *p,rc_state_reader *r){if(r->end-r->at<6||rc_sr_u32(r)!=1)return 0;p->inputs=rc_sr_u16(r);return rc_sr_ok(r);}
static uint8_t read(void *d,unsigned space,uint32_t off){(void)space;(void)off;return rc_pla_decode(((rc_pla *)d)->inputs);}
static void reset(void *d){((rc_pla *)d)->inputs=0;}
static void save(const void *d,rc_state_writer *w){rc_pla_save(d,w);}static int load(void *d,rc_state_reader *r){return rc_pla_load(d,r);}
static const rc_dev_ops OPS={.read=read,.reset=reset,.save=save,.load=load};
void rc_pla_init(rc_pla *p,const rc_hw_host *host){p->dev=(rc_dev){&OPS,host,"PLA"};p->inputs=0;}
