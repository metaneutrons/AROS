/*
 * Author: Fabian Schmieder
 * Synopsys DesignWare I2C HIDD for RP1 (Raspberry Pi 5)
 *
 * Register definitions from Linux drivers/i2c/busses/i2c-designware-core.h
 * RP1 I2C0 at BAR1 + 0x070000, I2C1 at BAR1 + 0x074000
 */

#ifndef I2C_DW_H
#define I2C_DW_H

#include <exec/types.h>
#include <exec/libraries.h>
#include <oop/oop.h>
#include <aros/macros.h>

/* DesignWare I2C register offsets */
#define DW_IC_CON           0x00
#define DW_IC_TAR           0x04
#define DW_IC_DATA_CMD      0x10
#define DW_IC_SS_SCL_HCNT   0x14
#define DW_IC_SS_SCL_LCNT   0x18
#define DW_IC_FS_SCL_HCNT   0x1C
#define DW_IC_FS_SCL_LCNT   0x20
#define DW_IC_INTR_STAT     0x2C
#define DW_IC_INTR_MASK     0x30
#define DW_IC_RAW_INTR_STAT 0x34
#define DW_IC_CLR_INTR      0x40
#define DW_IC_ENABLE        0x6C
#define DW_IC_STATUS        0x70
#define DW_IC_TXFLR         0x74
#define DW_IC_RXFLR         0x78
#define DW_IC_COMP_PARAM_1  0xF4
#define DW_IC_ENABLE_STATUS 0x9C

/* DW_IC_CON bits */
#define IC_CON_MASTER       (1 << 0)
#define IC_CON_SPEED_STD    (1 << 1)
#define IC_CON_SPEED_FAST   (2 << 1)
#define IC_CON_10BIT_SLAVE  (1 << 3)
#define IC_CON_RESTART_EN   (1 << 5)
#define IC_CON_SLAVE_DIS    (1 << 6)

/* DW_IC_DATA_CMD bits */
#define IC_DATA_CMD_READ    (1 << 8)
#define IC_DATA_CMD_STOP    (1 << 9)

/* DW_IC_STATUS bits */
#define IC_STATUS_ACTIVITY  (1 << 0)
#define IC_STATUS_TFNF      (1 << 1)  /* TX FIFO not full */
#define IC_STATUS_TFE       (1 << 2)  /* TX FIFO empty */
#define IC_STATUS_RFNE      (1 << 3)  /* RX FIFO not empty */

/* DW_IC_ENABLE bits */
#define IC_ENABLE_ENABLE    (1 << 0)

/* Register access */
static inline ULONG dw_i2c_rd(IPTR base, ULONG off)
{
    return AROS_LE2LONG(*(volatile ULONG *)(base + off));
}

static inline void dw_i2c_wr(IPTR base, ULONG off, ULONG val)
{
    *(volatile ULONG *)(base + off) = AROS_LONG2LE(val);
}

struct I2CDWBase {
    struct Library  i2c_LibNode;
    OOP_Class       *i2c_DrvClass;
    IPTR            i2c_RegBase;
};

#define METHOD(base, id, name) \
  base ## __ ## id ## __ ## name (OOP_Class *cl, OOP_Object *o, struct p ## id ## _ ## name *msg)

#endif /* I2C_DW_H */
