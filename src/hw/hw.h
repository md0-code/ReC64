/* Portable chip contract: chips call only their board host. Time is CPU
 * cycles; the board owns address decode and phi1/phi2 bus arbitration. */
#ifndef RC_HW_H
#define RC_HW_H
#include <stddef.h>
#include <stdint.h>
#define RC_NEVER UINT64_MAX
enum { RC_SPACE_MEM, RC_SPACE_IO };
struct rc_state_writer;
struct rc_state_reader;
typedef struct rc_dev_ops {
    uint8_t (*read)(void *dev, unsigned space, uint32_t offset);
    void (*write)(void *dev, unsigned space, uint32_t offset, uint8_t value);
    void (*event)(void *dev, uint64_t now);
    void (*reset)(void *dev);
    void (*input)(void *dev, int pin, int state);
    void (*save)(const void *dev, struct rc_state_writer *w);
    int (*load)(void *dev, struct rc_state_reader *r);
} rc_dev_ops;
typedef struct rc_hw_host {
    void *ctx;
    uint64_t (*now)(void *ctx);
    void (*schedule)(void *ctx, void *dev, uint64_t when);
    void (*line)(void *ctx, void *dev, int pin, int state);
} rc_hw_host;
typedef struct rc_dev {
    const rc_dev_ops *ops;
    const rc_hw_host *host;
    const char *name;
} rc_dev;
static inline uint64_t rc_dev_now(const rc_dev *d) { return d->host->now(d->host->ctx); }
static inline void rc_dev_schedule(rc_dev *d, uint64_t when) { d->host->schedule(d->host->ctx, d, when); }
static inline void rc_dev_line(rc_dev *d, int pin, int state) { d->host->line(d->host->ctx, d, pin, state); }
/* The CPU exposes one bus access per cycle. The board supplies read data
 * and advances the CPU only on a granted cycle; RDY holds a read's pins. */
typedef struct rc_cpu_cycle {
    uint16_t address;
    uint8_t data, write, sync;
} rc_cpu_cycle;
typedef struct rc_cpu_hooks {
    void *ctx;
    const uint8_t *watch; /* 8192 bytes, one bit per PC */
    int (*before_fetch)(void *ctx, uint16_t pc);
    void (*after_fetch)(void *ctx, uint16_t pc, uint8_t opcode);
} rc_cpu_hooks;
static inline int rc_cpu_watched(const rc_cpu_hooks *h, uint16_t pc) {
    return h && h->watch && (h->watch[pc >> 3] >> (pc & 7u) & 1u);
}
enum { RC_DRIVE_MOTOR, RC_DRIVE_STEP, RC_DRIVE_HEAD_LOAD, RC_DRIVE_HEAD_HOLD };
typedef void (*rc_drive_hook)(void *ctx, unsigned drive, unsigned event, uint64_t t, uint32_t a, uint32_t b);
static inline uint16_t rc_get16(const uint8_t *p) { return (uint16_t)(p[0] | (uint16_t)p[1] << 8); }
static inline void rc_put16(uint8_t *p, uint16_t v) { p[0]=(uint8_t)v; p[1]=(uint8_t)(v>>8); }
#endif
