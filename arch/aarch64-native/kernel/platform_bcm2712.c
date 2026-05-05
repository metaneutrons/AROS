/*
    Copyright (C) 2026, The AROS Development Team. All rights reserved.

    Desc: BCM2712 (Raspberry Pi 5) platform probe and serial output.

    The BCM2712 uses a Cortex-A76 (MIDR part 0xD0B) and has peripherals
    at a different base address. Most I/O is behind the RP1 southbridge
    chip connected via PCIe, which is initialized later in the boot
    sequence by the RP1 driver.

    Serial output uses the BCM2712's own PL011 UART (not RP1).
*/

#include <stdint.h>
#include "kernel_intern.h"

/*
 * BCM2712 has its own PL011 at a different offset.
 * The firmware initializes UART and we just use it.
 */
#define BCM2712_UART_BASE       (BCM2712_PERIBASE + 0x201000)

#define PL011_DR                0x00
#define PL011_FR                0x18
#define PL011_FR_TXFF           (1 << 5)
#define PL011_FR_RXFE           (1 << 4)

static inline void mmio_wr(unsigned long addr, uint32_t val)
{
    *(volatile uint32_t *)addr = val;
}

static inline uint32_t mmio_rd(unsigned long addr)
{
    return *(volatile uint32_t *)addr;
}

static void bcm2712_ser_putc(uint8_t chr)
{
    int timeout = 100000;
    while (timeout-- > 0 && (mmio_rd(BCM2712_UART_BASE + PL011_FR) & PL011_FR_TXFF))
        ;
    if (chr == '\n')
    {
        mmio_wr(BCM2712_UART_BASE + PL011_DR, '\r');
        timeout = 100000;
        while (timeout-- > 0 && (mmio_rd(BCM2712_UART_BASE + PL011_FR) & PL011_FR_TXFF))
            ;
    }
    mmio_wr(BCM2712_UART_BASE + PL011_DR, chr);
}

static int bcm2712_ser_getc(void)
{
    if (!(mmio_rd(BCM2712_UART_BASE + PL011_FR) & PL011_FR_RXFE))
        return (int)mmio_rd(BCM2712_UART_BASE + PL011_DR);
    return -1;
}

/*
 * bcm2712_probe — detect BCM2712 SoC via MIDR_EL1.
 *
 * Cortex-A76 part number = 0xD0B (ARM ARM D13.2.89).
 */
int bcm2712_probe(struct AARCH64_Implementation *impl, void *bootmsg)
{
    uint64_t midr;
    __asm__ volatile("mrs %0, midr_el1" : "=r"(midr));

    /* Cortex-A76: implementer=0x41 (ARM), part=0xD0B */
    if (((midr >> 4) & 0xFFF) != 0xD0B)
        return 0;

    impl->ARMI_Family        = 8;
    impl->ARMI_Platform      = 0x2712;
    impl->ARMI_PeripheralBase = (void *)BCM2712_PERIBASE;
    impl->ARMI_SerPutChar    = bcm2712_ser_putc;
    impl->ARMI_SerGetChar    = bcm2712_ser_getc;

    return 1;
}
