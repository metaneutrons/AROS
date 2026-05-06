#ifndef AHI_Drivers_RPiHDMI_DriverData_h
#define AHI_Drivers_RPiHDMI_DriverData_h

#include <exec/libraries.h>
#include <dos/dos.h>
#include <proto/dos.h>

#include "DriverBase.h"

/*
 * BCM2835 DMA Control Block - must be 32-byte aligned
 */
struct DMAControlBlock {
    ULONG ti;          /* Transfer Information */
    ULONG source_ad;   /* Source address (bus address) */
    ULONG dest_ad;     /* Destination address (bus address) */
    ULONG txfr_len;    /* Transfer length in bytes */
    ULONG stride;      /* 2D stride */
    ULONG nextconbk;   /* Next CB address (bus address), 0 = stop */
    ULONG reserved[2]; /* Padding to 32 bytes */
};

/*
 * SoC variant for register offset selection
 */
enum RPiHDMIVariant {
    VARIANT_BCM2835 = 0,  /* RPi 1/2/3: peribase 0x20000000 / 0x3F000000 */
    VARIANT_BCM2711 = 1,  /* RPi4: peribase 0xFE000000 */
    VARIANT_BCM2712 = 2,  /* RPi5: peribase 0x107C000000 */  /* RPi 4:     peribase 0xFE000000 */
};

/*
 * Driver library base
 */
struct RPiHDMIBase {
    struct DriverBase driverbase;
    struct DosLibrary *dosbase;
    IPTR periiobase;
    enum RPiHDMIVariant variant;
};

#define DRIVERBASE_SIZEOF (sizeof(struct RPiHDMIBase))

#define DOSBase (*(struct DosLibrary **) &RPiHDMIBase->dosbase)

/*
 * Per-audio-context driver data (stored in ahiac_DriverData)
 */
struct RPiHDMIData {
    struct DriverData driverdata;
    UBYTE flags;
    UBYTE pad1;
    BYTE mastersignal;
    BYTE slavesignal;
    struct Process *mastertask;
    struct Process *slavetask;
    struct RPiHDMIBase *ahisubbase;

    /* Hardware state */
    IPTR periiobase;
    enum RPiHDMIVariant variant;
    ULONG dma_channel;

    /* Register base addresses (computed from periiobase + variant) */
    IPTR hd_base;    /* HD block (MAI registers) */
    IPTR hdmi_base;  /* HDMI core block */
    IPTR ram_base;   /* RAM packet block (BCM2711: separate; BCM2835: inside hdmi_base) */

    /* DMA control blocks (32-byte aligned) */
    struct DMAControlBlock *cb_base; /* Allocated block (for free) */
    struct DMAControlBlock *cb[2];   /* Aligned pointers to CB A and CB B */

    /* Audio buffers */
    APTR mixbuffer;       /* AHI mix buffer (signed 16-bit) */
    ULONG *dmabuf[2];     /* SPDIF DMA buffers (32-bit subframes) */
    ULONG dmabuf_size;    /* Size of each DMA buffer in bytes */
    ULONG dmabuf_samples; /* Number of sample frames per buffer */

    /* IRQ */
    APTR irq_handle;

    /* Configuration */
    ULONG samplerate;

    /* IEC958 state */
    UBYTE channel_status_l[24];
    UBYTE channel_status_r[24];
    ULONG frame_counter;
};

#endif /* AHI_Drivers_RPiHDMI_DriverData_h */
