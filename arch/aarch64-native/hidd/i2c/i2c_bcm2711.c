/*
 * Copyright (C) 2026, The AROS Development Team. All rights reserved.
 * Author: Fabian Schmieder
 */
/*
 * BCM2711 I2C (BSC1) HIDD driver for Raspberry Pi 4
 *
 * Provides Hidd_I2C interface for the BSC1 controller
 * on GPIO 2 (SDA) / GPIO 3 (SCL).
 */

#include <aros/symbolsets.h>
#include <aros/debug.h>
#include <aros/macros.h>
#include <proto/exec.h>
#include <proto/kernel.h>
#include <proto/oop.h>
#include <hidd/i2c.h>

#include "i2c_bcm2711.h"

#include LC_LIBDEFS_FILE



APTR KernelBase __attribute__((used)) = NULL;

static void bsc_wait_done(IPTR base)
{
    while (!(bsc_rd(base, BSC_STATUS) & BSC_STA_DONE))
        ;
}

/*
 * PutByte — write a single byte to the current I2C device.
 */
void METHOD(I2CBCM2711, Hidd_I2C, PutByte)
{
    struct I2CBCM2711Base *LIBBASE = (struct I2CBCM2711Base *)cl->UserData;
    IPTR base = LIBBASE->i2c_RegBase;

    bsc_wait_done(base);
    bsc_wr(base, BSC_DATALEN, 1);
    bsc_wr(base, BSC_STATUS, BSC_CLEAR_STATUS);
    bsc_wr(base, BSC_FIFO, msg->data);
    bsc_wr(base, BSC_CONTROL, BSC_CTL_I2CEN | BSC_CTL_START);
    bsc_wait_done(base);
}

/*
 * GetByte — read a single byte from the current I2C device.
 */
void METHOD(I2CBCM2711, Hidd_I2C, GetByte)
{
    struct I2CBCM2711Base *LIBBASE = (struct I2CBCM2711Base *)cl->UserData;
    IPTR base = LIBBASE->i2c_RegBase;

    bsc_wait_done(base);
    bsc_wr(base, BSC_DATALEN, 1);
    bsc_wr(base, BSC_STATUS, BSC_CLEAR_STATUS);
    bsc_wr(base, BSC_CONTROL, BSC_CTL_I2CEN | BSC_CTL_START | BSC_CTL_READ);
    bsc_wait_done(base);

    *(msg->data) = (UBYTE)bsc_rd(base, BSC_FIFO);
}

/*
 * WriteRead — combined write+read transaction.
 */
void METHOD(I2CBCM2711, Hidd_I2C, WriteRead)
{
    struct I2CBCM2711Base *LIBBASE = (struct I2CBCM2711Base *)cl->UserData;
    IPTR base = LIBBASE->i2c_RegBase;
    ULONG i;

    /* Write phase */
    bsc_wait_done(base);
    bsc_wr(base, BSC_DATALEN, msg->writeLength);
    bsc_wr(base, BSC_STATUS, BSC_CLEAR_STATUS);
    for (i = 0; i < msg->writeLength; i++)
        bsc_wr(base, BSC_FIFO, msg->writeBuffer[i]);
    bsc_wr(base, BSC_CONTROL, BSC_CTL_I2CEN | BSC_CTL_START);
    bsc_wait_done(base);

    /* Read phase */
    bsc_wr(base, BSC_DATALEN, msg->readLength);
    bsc_wr(base, BSC_STATUS, BSC_CLEAR_STATUS);
    bsc_wr(base, BSC_CONTROL, BSC_CTL_I2CEN | BSC_CTL_START | BSC_CTL_READ);
    bsc_wait_done(base);

    for (i = 0; i < msg->readLength; i++)
        msg->readBuffer[i] = (UBYTE)bsc_rd(base, BSC_FIFO);
}

/*
 * Initialization — set up GPIO pins for I2C and enable BSC1.
 */
static int I2CBCM2711_Init(LIBBASETYPEPTR LIBBASE)
{
    IPTR peribase = KrnGetSystemAttr(KATTR_PeripheralBase);
    IPTR gpio_base = peribase + GPIO_OFFSET;
    ULONG tmp;

    D(bug("[I2C] BCM2711 I2C Init\n"));

    KernelBase = OpenResource("kernel.resource");
    if (!KernelBase)
        return FALSE;

    LIBBASE->i2c_RegBase = peribase + BSC1_OFFSET;

    /* Set GPIO 2/3 to ALT0 (I2C1 SDA/SCL) */
    tmp = AROS_LE2LONG(*(volatile ULONG *)(gpio_base + GPFSEL0_OFF));
    tmp &= ~((7 << 6) | (7 << 9));  /* Clear FSEL2 and FSEL3 */
    tmp |= (4 << 6) | (4 << 9);     /* ALT0 = 0b100 for both */
    *(volatile ULONG *)(gpio_base + GPFSEL0_OFF) = AROS_LONG2LE(tmp);

    /* Enable BSC1, set clock to ~100kHz (divider = 1500 at 150MHz core clock) */
    bsc_wr(LIBBASE->i2c_RegBase, BSC_CLKDIV, 1500);
    bsc_wr(LIBBASE->i2c_RegBase, BSC_CONTROL, BSC_CTL_I2CEN);

    D(bug("[I2C] BSC1 enabled at 0x%p, ~100kHz\n", LIBBASE->i2c_RegBase));

    return TRUE;
}

ADD2INITLIB(I2CBCM2711_Init, 0)
