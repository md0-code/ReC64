/* Open-collector IEC bus: a set bit means the physical line is released. */
#ifndef RC_IEC_H
#define RC_IEC_H
#include "../core/state.h"
enum {RC_IEC_DATA=1,RC_IEC_CLK=2,RC_IEC_ATN=4,RC_IEC_RESET=8,RC_IEC_ALL=15};
typedef struct rc_iec {uint8_t output[2],lines;uint64_t edges;} rc_iec;
void rc_iec_init(rc_iec *b);
void rc_iec_output(rc_iec *b,unsigned endpoint,unsigned released);
unsigned rc_iec_lines(const rc_iec *b);
void rc_iec_save(const rc_iec *b,rc_state_writer *w);
int rc_iec_load(rc_iec *b,rc_state_reader *r);
#endif
