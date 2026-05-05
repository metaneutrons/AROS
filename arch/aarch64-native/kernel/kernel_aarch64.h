/*
    Copyright (C) 2026, The AROS Development Team. All rights reserved.

    Desc: AArch64 hardware abstraction — equivalent to kernel_arm.h
*/

#ifndef __KERNEL_AARCH64_H
#define __KERNEL_AARCH64_H

#include <inttypes.h>
#include <asm/cpu.h>
#include <aros/cpu.h>

struct AARCH64_Implementation
{
    IPTR                ARMI_Family;
    IPTR                ARMI_Platform;
    APTR                ARMI_PeripheralBase;
    cpumask_t           ARMI_AffinityMask;
    void                (*ARMI_Init)(APTR, APTR);
    void                (*ARMI_InitCore)(APTR, APTR);
    void                (*ARMI_SendIPI)(uint32_t, uint32_t, uint32_t);
    APTR                (*ARMI_InitTimer)(APTR);
    void                (*ARMI_Delay)(int);
    unsigned int        (*ARMI_GetTime)(void);
    void                (*ARMI_PutChar)(int);
    void                (*ARMI_SerPutChar)(uint8_t);
    int                 (*ARMI_SerGetChar)(void);
    void                (*ARMI_IRQInit)(void);
    void                (*ARMI_IRQEnable)(int);
    void                (*ARMI_IRQDisable)(int);
    void                (*ARMI_IRQProcess)(void);
    void                (*ARMI_FIQProcess)(void);
    void                (*ARMI_LED_Toggle)(int, int);
    void                (*ARMI_Save_VFP_State)(void *);
    void                (*ARMI_Restore_VFP_State)(void *);
    void                (*ARMI_Init_VFP_State)(void *);
};

extern struct AARCH64_Implementation __aarch64_arosintern;

#define ARM_LED_ON          1
#define ARM_LED_OFF         0
#define ARM_LED_POWER       0
#define ARM_LED_ACTIVITY    1

/* BCM2711 (Pi 4) hardware addresses */
#define BCM2711_PERIBASE        0xFE000000UL
#define BCM2711_GICD_BASE       0xFF841000UL
#define BCM2711_GICC_BASE       0xFF842000UL

/* BCM2712 (Pi 5) hardware addresses */
#define BCM2712_PERIBASE        0x107C000000ULL
#define BCM2712_GICD_BASE       0xFF841000UL
#define BCM2712_GICC_BASE       0xFF842000UL

/* RP1 southbridge (PCIe-attached on Pi 5) */
#define RP1_PCIE_VENDOR_ID      0x1de4
#define RP1_PCIE_DEVICE_ID      0x0001
#define RP1_BAR1_SIZE           0x400000  /* 4MB peripheral space */

/* RP1 peripheral offsets within BAR1 */
#define RP1_USB0_OFFSET         0x100000
#define RP1_USB1_OFFSET         0x110000
#define RP1_ETH_OFFSET          0x180000
#define RP1_GPIO_OFFSET         0x0D0000
#define RP1_I2C0_OFFSET         0x070000
#define RP1_I2C1_OFFSET         0x074000
#define RP1_I2C2_OFFSET         0x078000
#define RP1_SPI0_OFFSET         0x050000
#define RP1_UART0_OFFSET        0x030000
#define RP1_UART1_OFFSET        0x034000

#endif /* __KERNEL_AARCH64_H */
