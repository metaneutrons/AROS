#ifndef I2C_BCM2711_H
 * Author: Fabian Schmieder
#define I2C_BCM2711_H

#include <exec/types.h>
#include <exec/libraries.h>
#include <oop/oop.h>
#include <aros/macros.h>

/*
 * BCM2711 BSC (Broadcom Serial Controller) I2C registers.
 * BSC1 is the user-accessible I2C on GPIO 2 (SDA) / GPIO 3 (SCL).
 * Register layout is identical to BCM2835.
 */
#define BSC1_OFFSET     0x804000

#define BSC_CONTROL     0x00
#define BSC_STATUS      0x04
#define BSC_DATALEN     0x08
#define BSC_SLAVE_ADDR  0x0C
#define BSC_FIFO        0x10
#define BSC_CLKDIV      0x14
#define BSC_DELAY       0x18
#define BSC_CLKSTRETCH  0x1C

/* BSC_CONTROL bits */
#define BSC_CTL_READ    (1 << 0)
#define BSC_CTL_CLEAR   (3 << 4)
#define BSC_CTL_START   (1 << 7)
#define BSC_CTL_INTD    (1 << 8)
#define BSC_CTL_INTT    (1 << 9)
#define BSC_CTL_INTR    (1 << 10)
#define BSC_CTL_I2CEN   (1 << 15)

/* BSC_STATUS bits */
#define BSC_STA_TA      (1 << 0)
#define BSC_STA_DONE    (1 << 1)
#define BSC_STA_TXW     (1 << 2)
#define BSC_STA_RXR     (1 << 3)
#define BSC_STA_TXD     (1 << 4)
#define BSC_STA_RXD     (1 << 5)
#define BSC_STA_TXE     (1 << 6)
#define BSC_STA_RXF     (1 << 7)
#define BSC_STA_ERR     (1 << 8)
#define BSC_STA_CLKT    (1 << 9)

/* Clear flags */
#define BSC_CLEAR_STATUS (BSC_STA_DONE | BSC_STA_ERR | BSC_STA_CLKT)

/* GPIO function select for I2C1 (ALT0 on GPIO 2/3) */
#define GPIO_OFFSET     0x200000
#define GPFSEL0_OFF     0x00

/* Register access helpers */
static inline ULONG bsc_rd(IPTR base, ULONG off)
{
    return AROS_LE2LONG(*(volatile ULONG *)(base + off));
}

static inline void bsc_wr(IPTR base, ULONG off, ULONG val)
{
    *(volatile ULONG *)(base + off) = AROS_LONG2LE(val);
}

struct I2CBCM2711Base {
    struct Library  i2c_LibNode;
    OOP_Class       *i2c_DrvClass;
    IPTR            i2c_RegBase;    /* BSC1 register base */
};

#define METHOD(base, id, name) \
  base ## __ ## id ## __ ## name (OOP_Class *cl, OOP_Object *o, struct p ## id ## _ ## name *msg)

#endif /* I2C_BCM2711_H */
