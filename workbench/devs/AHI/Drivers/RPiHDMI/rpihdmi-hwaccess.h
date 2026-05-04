#ifndef AHI_Drivers_RPiHDMI_hwaccess_h
#define AHI_Drivers_RPiHDMI_hwaccess_h

#include <exec/types.h>
#include <aros/macros.h>

#include "DriverData.h"

/*
 * GPU bus address for DMA.
 *
 * BCM2835/2836/2837 (RPi 1/2/3):
 *   ARM physical → GPU bus via uncached alias 0xC0000000.
 *   peribase 0x20000000 or 0x3F000000.
 *
 * BCM2711 (RPi 4):
 *   The "legacy" DMA controller (channels 0-6) uses a 32-bit address
 *   space where ARM physical addresses in the low 1GB map directly
 *   (the 0xC0000000 alias also works for addresses < 0x40000000).
 *   Peripheral registers at bus 0x7E000000 map to ARM 0xFE000000.
 *   For DMA to peripherals, use the bus address directly (0x7Exxxxxx).
 *   For DMA from RAM, addresses < 0x40000000 use 0xC0000000 | addr.
 */
static inline ULONG gpu_bus_addr(IPTR arm_phys)
{
    return 0xC0000000 | (ULONG)(arm_phys & 0x3FFFFFFF);
}

/* MAI DATA bus address (DMA destination) — same for both variants */
#define HDMI_MAI_DATA_BUS_BCM2835 0x7E808020
#define HDMI_MAI_DATA_BUS_BCM2711 0x7E80001C

/* Register access helpers with memory barriers */
static inline void __dsb(void)
{
#ifdef __aarch64__
    asm volatile("dsb sy" ::: "memory");
#else
    asm volatile("dsb" ::: "memory");
#endif
}

static inline void __dmb(void)
{
#ifdef __aarch64__
    asm volatile("dmb sy" ::: "memory");
#else
    asm volatile("dmb" ::: "memory");
#endif
}

static inline ULONG rd32le(IPTR addr)
{
    ULONG val;
    __dmb();
    val = AROS_LE2LONG(*(volatile ULONG *) addr);
    __dsb();
    return val;
}

static inline void wr32le(IPTR addr, ULONG val)
{
    __dsb();
    *(volatile ULONG *) addr = AROS_LONG2LE(val);
    __dmb();
}

/*
 * Register offsets for BCM2835 (VC4) — HD block at peribase + 0x808000
 */
#define VC4_HD_OFFSET       0x808000
#define VC4_HDMI_OFFSET     0x902000

#define VC4_HD_MAI_CTL      0x0014
#define VC4_HD_MAI_THR      0x0018
#define VC4_HD_MAI_FMT      0x001C
#define VC4_HD_MAI_DATA     0x0020
#define VC4_HD_MAI_SMP      0x002C

#define VC4_HDMI_MAI_CHANNEL_MAP   0x0090
#define VC4_HDMI_MAI_CONFIG        0x0094
#define VC4_HDMI_AUDIO_PKT_CFG     0x009C
#define VC4_HDMI_RAM_PKT_CFG       0x00A0
#define VC4_HDMI_RAM_PKT_STATUS    0x00A4
#define VC4_HDMI_CRP_CFG           0x00A8
#define VC4_HDMI_CTS_0             0x00AC
#define VC4_HDMI_CTS_1             0x00B0
#define VC4_HDMI_SCHEDULER_CONTROL 0x00C0
#define VC4_HDMI_RAM_PKT_START     0x0400

/*
 * Register offsets for BCM2711 (VC5) HDMI0 — HD block at peribase + 0x800000
 * From Linux vc5_hdmi_hdmi0_fields[].
 */
#define VC5_HD_OFFSET       0x800000
#define VC5_HDMI_OFFSET     0x802000
#define VC5_RAM_OFFSET      0x802000  /* RAM packets in HDMI core block for VC5 */

#define VC5_HD_MAI_CTL      0x0010
#define VC5_HD_MAI_THR      0x0014
#define VC5_HD_MAI_FMT      0x0018
#define VC5_HD_MAI_DATA     0x001C
#define VC5_HD_MAI_SMP      0x0020

#define VC5_HDMI_MAI_CHANNEL_MAP   0x009C
#define VC5_HDMI_MAI_CONFIG        0x00A0
#define VC5_HDMI_AUDIO_PKT_CFG     0x00B8
#define VC5_HDMI_RAM_PKT_CFG       0x00BC
#define VC5_HDMI_RAM_PKT_STATUS    0x00C4
#define VC5_HDMI_CRP_CFG           0x00C8
#define VC5_HDMI_CTS_0             0x00CC
#define VC5_HDMI_CTS_1             0x00D0
#define VC5_HDMI_SCHEDULER_CONTROL 0x00E0
#define VC5_HDMI_RAM_PKT_START     0x0400  /* Relative to RAM base */

/*
 * Accessor macros that select the correct offset based on variant.
 * dd->hd_base, dd->hdmi_base, dd->ram_base are pre-computed.
 */
#define HD_REG(dd, vc4_off, vc5_off) \
    ((dd)->hd_base + ((dd)->variant == VARIANT_BCM2711 ? (vc5_off) : (vc4_off)))

#define HDMI_REG(dd, vc4_off, vc5_off) \
    ((dd)->hdmi_base + ((dd)->variant == VARIANT_BCM2711 ? (vc5_off) : (vc4_off)))

/* Convenience register accessors */
#define REG_MAI_CTL(dd)          HD_REG(dd, VC4_HD_MAI_CTL, VC5_HD_MAI_CTL)
#define REG_MAI_THR(dd)          HD_REG(dd, VC4_HD_MAI_THR, VC5_HD_MAI_THR)
#define REG_MAI_FMT(dd)          HD_REG(dd, VC4_HD_MAI_FMT, VC5_HD_MAI_FMT)
#define REG_MAI_DATA(dd)         HD_REG(dd, VC4_HD_MAI_DATA, VC5_HD_MAI_DATA)
#define REG_MAI_SMP(dd)          HD_REG(dd, VC4_HD_MAI_SMP, VC5_HD_MAI_SMP)
#define REG_MAI_CHANNEL_MAP(dd)  HDMI_REG(dd, VC4_HDMI_MAI_CHANNEL_MAP, VC5_HDMI_MAI_CHANNEL_MAP)
#define REG_MAI_CONFIG(dd)       HDMI_REG(dd, VC4_HDMI_MAI_CONFIG, VC5_HDMI_MAI_CONFIG)
#define REG_AUDIO_PKT_CFG(dd)    HDMI_REG(dd, VC4_HDMI_AUDIO_PKT_CFG, VC5_HDMI_AUDIO_PKT_CFG)
#define REG_RAM_PKT_CFG(dd)      HDMI_REG(dd, VC4_HDMI_RAM_PKT_CFG, VC5_HDMI_RAM_PKT_CFG)
#define REG_RAM_PKT_STATUS(dd)   HDMI_REG(dd, VC4_HDMI_RAM_PKT_STATUS, VC5_HDMI_RAM_PKT_STATUS)
#define REG_CRP_CFG(dd)          HDMI_REG(dd, VC4_HDMI_CRP_CFG, VC5_HDMI_CRP_CFG)
#define REG_CTS_0(dd)            HDMI_REG(dd, VC4_HDMI_CTS_0, VC5_HDMI_CTS_0)
#define REG_CTS_1(dd)            HDMI_REG(dd, VC4_HDMI_CTS_1, VC5_HDMI_CTS_1)
#define REG_SCHEDULER_CONTROL(dd) HDMI_REG(dd, VC4_HDMI_SCHEDULER_CONTROL, VC5_HDMI_SCHEDULER_CONTROL)

/* RAM packet start — on BCM2711 this is at a fixed offset in the HDMI core block */
#define REG_RAM_PKT_START(dd) \
    ((dd)->variant == VARIANT_BCM2711 \
        ? ((dd)->hdmi_base + VC5_HDMI_RAM_PKT_START) \
        : ((dd)->hdmi_base + VC4_HDMI_RAM_PKT_START))

/* MAI_CTL bits */
#define MAI_CTL_RESET    (1 << 0)
#define MAI_CTL_ERRORF   (1 << 1)
#define MAI_CTL_ERRORE   (1 << 2)
#define MAI_CTL_ENABLE   (1 << 3)
#define MAI_CTL_CHNUM(x) (((x) & 0xF) << 4)
#define MAI_CTL_PAREN    (1 << 8)
#define MAI_CTL_FLUSH    (1 << 9)
#define MAI_CTL_WHOLSMP  (1 << 12)
#define MAI_CTL_CHALIGN  (1 << 13)
#define MAI_CTL_DLATE    (1 << 15)

/* MAI_THR: DREQ and panic thresholds */
#define MAI_THR_DREQL(x)  (((x) & 0x3F) << 0)
#define MAI_THR_DREQH(x)  (((x) & 0x3F) << 8)
#define MAI_THR_PANICL(x) (((x) & 0x3F) << 16)
#define MAI_THR_PANICH(x) (((x) & 0x3F) << 24)

/* MAI_FMT: sample rate and audio format */
#define MAI_FMT_RATE(x)    (((x) & 0xFF) << 8)
#define MAI_FMT_FORMAT(x)  (((x) & 0xFF) << 16)
#define MAI_FMT_FORMAT_PCM MAI_FMT_FORMAT(2)

/* MAI_CONFIG bits (HDMI block) */
#define MAI_CONFIG_BIT_REVERSE     (1 << 26)
#define MAI_CONFIG_FORMAT_REVERSE  (1 << 27)
#define MAI_CONFIG_CHANNEL_MASK(x) ((x) & 0xFFFF)

/* Audio packet config bits */
#define AUDIO_PKT_CEA_MASK(x)           ((x) & 0xFF)
#define AUDIO_PKT_B_FRAME_ID(x)         (((x) & 0xF) << 10)
#define AUDIO_PKT_ZERO_DATA_ON_INACTIVE (1 << 24)
#define AUDIO_PKT_ZERO_DATA_ON_FLAT     (1 << 29)

/* CRP_CFG bits */
#define CRP_CFG_EXTERNAL_CTS_EN (1 << 24)
#define CRP_CFG_N(x)            ((x) & 0xFFFFF)

/* DMA channel for HDMI audio */
#define HDMI_DMA_CHANNEL 6

/* DMA DREQ peripheral map ID for HDMI audio */
#define DMA_DREQ_HDMI 17

/* BCM2835 DMA IRQ: channel N uses GPU IRQ (16 + N) */
#define BCM_IRQ_DMA0 16

/* DMA control block TI bits */
#define DMA_TI_INTEN           (1 << 0)
#define DMA_TI_WAIT_RESP       (1 << 3)
#define DMA_TI_DEST_DREQ       (1 << 6)
#define DMA_TI_SRC_INC         (1 << 8)
#define DMA_TI_BURST_LENGTH(x) (((x) & 0xF) << 12)
#define DMA_TI_PERMAP(x)       (((x) & 0x1F) << 16)
#define DMA_TI_NO_WIDE_BURSTS  (1 << 26)

/* DMA CS bits */
#define DMA_CS_ACTIVE          (1 << 0)
#define DMA_CS_END             (1 << 1)
#define DMA_CS_INT             (1 << 2)
#define DMA_CS_WAIT_FOR_WRITES (1 << 28)
#define DMA_CS_PANIC_PRI(x)    (((x) & 0xF) << 20)
#define DMA_CS_PRI(x)          (((x) & 0xF) << 16)
#define DMA_CS_ABORT           (1 << 30)
#define DMA_CS_RESET           (1U << 31)

/* Sample rate enum values for MAI_FMT */
#define SRATE_8000   1
#define SRATE_11025  2
#define SRATE_12000  3
#define SRATE_16000  4
#define SRATE_22050  5
#define SRATE_24000  6
#define SRATE_32000  7
#define SRATE_44100  8
#define SRATE_48000  9
#define SRATE_88200  10
#define SRATE_96000  12
#define SRATE_176400 14
#define SRATE_192000 15

/* Hardware setup/teardown */
void hdmi_mai_init(struct RPiHDMIData *dd);
void hdmi_mai_stop(struct RPiHDMIData *dd);

/* Compute register base addresses from periiobase + variant */
void hdmi_setup_bases(struct RPiHDMIData *dd);

/* DMA functions */
void dma_setup(struct RPiHDMIData *dd, ULONG cb_bus_addr);
void dma_stop(struct RPiHDMIData *dd);
void dma_build_control_blocks(struct RPiHDMIData *dd);

/* DMA interrupt handler */
void dma_irq_handler(struct RPiHDMIData *data, void *data2);

/* IEC958 channel status setup */
void spdif_setup_channel_status(UBYTE *cs_left, UBYTE *cs_right, ULONG samplerate);

/* IEC958 sample conversion */
void convert_mix_to_iec958(WORD *src, ULONG *dst, ULONG frames, UBYTE *cs_left, UBYTE *cs_right, ULONG *frame_counter);

#endif /* AHI_Drivers_RPiHDMI_hwaccess_h */
