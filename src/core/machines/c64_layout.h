/* C64 physical keycaps; SHIFT LOCK shares the left-shift matrix contact. */
#ifndef RC_C64_LAYOUT_H
#define RC_C64_LAYOUT_H
#include "../machine.h"
typedef struct rc_c64_keycap {uint8_t key,x,y,w,h;const char *legend;} rc_c64_keycap;
enum {RC_C64_LAYOUT_WIDTH=80,RC_C64_LAYOUT_HEIGHT=20};
/* Geometry is in quarter-key units. Keys retain CIA matrix order 0..63,
 * with RESTORE 64; the extra SHIFT LOCK cap also closes key 15. */
const rc_c64_keycap *rc_c64_layout(unsigned *count);
/* Fill existing family descriptors without changing names or host bindings. */
void rc_c64_layout_apply(rc_key_desc *keys,unsigned count);
#endif
