/* Optional known-ROM fetch hooks. Firmware is never changed. */
#ifndef RC_TRAPS_H
#define RC_TRAPS_H
#include "../core/state.h"

typedef struct rc_traps_io {
    void *ctx;
    uint8_t (*read)(void *ctx,uint16_t address);
    void (*write)(void *ctx,uint16_t address,uint8_t value);
    /* 0: no virtual device (execute the real ROM), 1: success,
     * 2: success with absolute tape-header address (KERNAL type3),
     * negative: KERNAL error 1..9. Data is borrowed through this call. */
    int (*load)(void *ctx,unsigned device,unsigned secondary,const uint8_t name[16],unsigned length,
                uint16_t *address,const uint8_t **data,size_t *size);
    int (*save)(void *ctx,unsigned device,unsigned secondary,const uint8_t name[16],unsigned length,
                uint16_t start,uint16_t end);
} rc_traps_io;
typedef struct rc_traps {
    uint8_t watch[8192],known,load_enabled,save_enabled;
    uint32_t loads,saves;
} rc_traps;
void rc_traps_init(rc_traps *t,const uint8_t kernal_sha256[32]);
void rc_traps_enable(rc_traps *t,int load,int save);
/* The board checks that KERNAL ROM is mapped, then supplies the register A
 * at the fetch. A handled call supplies a PC in the unmodified ROM. */
int rc_traps_fetch(rc_traps *t,const rc_traps_io *io,uint16_t pc,uint8_t a,uint16_t *next_pc);
void rc_traps_save(const rc_traps *t,rc_state_writer *w);
int rc_traps_load(rc_traps *t,rc_state_reader *r);
#endif
