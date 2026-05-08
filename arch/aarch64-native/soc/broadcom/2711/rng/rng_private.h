#ifndef RNG_PRIVATE_H
/* Author: Fabian Schmieder */
#define RNG_PRIVATE_H

#include <exec/nodes.h>

/*
 * BCM2711 RNG200 registers (at peribase + 0x104000).
 *
 * The RNG200 is a TRNG (True Random Number Generator) that provides
 * hardware entropy. It has a FIFO that fills automatically.
 */
#define RNG200_CTRL         0x00
#define RNG200_FIFO_DATA    0x20
#define RNG200_FIFO_COUNT   0x24

#define RNG200_CTRL_ENABLE  (1 << 1)

struct RNGBase {
    struct Node     rng_Node;
    IPTR            rng_RegBase;
};

#endif /* RNG_PRIVATE_H */
