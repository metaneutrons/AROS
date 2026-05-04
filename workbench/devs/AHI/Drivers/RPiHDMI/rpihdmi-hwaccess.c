/*
 *  BCM2835/BCM2711 HDMI Audio hardware access for Raspberry Pi
 *
 *  Supports both BCM2835 (RPi 1/2/3) and BCM2711 (RPi 4) register layouts.
 *  Register offsets are selected at runtime based on the detected SoC variant.
 */

#include <config.h>

#include <exec/types.h>
#include <aros/macros.h>

#include "rpihdmi-hwaccess.h"

/*
 * Microsecond delay using a busy loop on the system timer.
 */
static void udelay(IPTR peribase, ULONG us)
{
    volatile ULONG *clo = (volatile ULONG *)(peribase + 0x003004);
    ULONG start = AROS_LE2LONG(*clo);

    while ((AROS_LE2LONG(*clo) - start) < us)
        ;
}

/*
 * Compute register base addresses from periiobase and variant.
 */
void hdmi_setup_bases(struct RPiHDMIData *dd)
{
    if (dd->variant == VARIANT_BCM2711) {
        dd->hd_base   = dd->periiobase + VC5_HD_OFFSET;
        dd->hdmi_base = dd->periiobase + VC5_HDMI_OFFSET;
        dd->ram_base  = dd->periiobase + VC5_HDMI_OFFSET;
    } else {
        dd->hd_base   = dd->periiobase + VC4_HD_OFFSET;
        dd->hdmi_base = dd->periiobase + VC4_HDMI_OFFSET;
        dd->ram_base  = dd->periiobase + VC4_HDMI_OFFSET;
    }
}

/*
 * Map sample rate to MAI format enum value.
 */
static ULONG srate_to_mai_enum(ULONG samplerate)
{
    switch (samplerate) {
    case 8000:   return SRATE_8000;
    case 11025:  return SRATE_11025;
    case 12000:  return SRATE_12000;
    case 16000:  return SRATE_16000;
    case 22050:  return SRATE_22050;
    case 24000:  return SRATE_24000;
    case 32000:  return SRATE_32000;
    case 44100:  return SRATE_44100;
    case 48000:  return SRATE_48000;
    case 88200:  return SRATE_88200;
    case 96000:  return SRATE_96000;
    case 176400: return SRATE_176400;
    case 192000: return SRATE_192000;
    default:     return SRATE_48000;
    }
}

/*
 * IEC 60958-3 Table 3: sampling frequency for channel status byte 3.
 */
static UBYTE srate_to_iec958_cs3(ULONG samplerate)
{
    switch (samplerate) {
    case 22050:  return 0x04;
    case 44100:  return 0x00;
    case 88200:  return 0x08;
    case 176400: return 0x0C;
    case 24000:  return 0x06;
    case 48000:  return 0x02;
    case 96000:  return 0x0A;
    case 192000: return 0x0E;
    case 32000:  return 0x03;
    default:     return 0x02;
    }
}

/*
 * HDMI audio clock recovery N value (HDMI spec Table 7-1/7-2/7-3).
 */
static ULONG srate_to_n(ULONG samplerate)
{
    switch (samplerate) {
    case 32000:  return 4096;
    case 44100:  return 6272;
    case 48000:  return 6144;
    case 88200:  return 12544;
    case 96000:  return 12288;
    case 176400: return 25088;
    case 192000: return 24576;
    default:     return 128 * samplerate / 1000;
    }
}


/******************************************************************************
** IEC958 channel status setup ************************************************
******************************************************************************/

void spdif_setup_channel_status(UBYTE *cs_left, UBYTE *cs_right, ULONG samplerate)
{
    int i;

    for (i = 0; i < 24; i++)
        cs_left[i] = cs_right[i] = 0;

    cs_left[0] = cs_right[0] = 0x04; /* Consumer, PCM, no copyright */
    cs_left[1] = cs_right[1] = 0x00; /* Category code = general */
    cs_left[3] = cs_right[3] = srate_to_iec958_cs3(samplerate);
    cs_left[4] = cs_right[4] = 0x02; /* 16-bit word length */

    cs_left[2]  = 0x10; /* Channel 1 (left) */
    cs_right[2] = 0x20; /* Channel 2 (right) */
}


/******************************************************************************
** IEC958 subframe encoding ***************************************************
******************************************************************************/

static inline ULONG iec958_parity(ULONG subframe)
{
    ULONG v = (subframe >> 4) & 0x07FFFFFF;
    v ^= v >> 16;
    v ^= v >> 8;
    v ^= v >> 4;
    v ^= v >> 2;
    v ^= v >> 1;
    return v & 1;
}

static inline ULONG encode_iec958_subframe(WORD sample, UBYTE cs_bit, UBYTE preamble)
{
    ULONG subframe;

    subframe = ((ULONG)(UWORD)sample) << 12;
    subframe |= (preamble & 0x0F);

    if (cs_bit)
        subframe |= (1 << 30);

    if (iec958_parity(subframe))
        subframe |= (1U << 31);

    return subframe;
}

void convert_mix_to_iec958(WORD *src, ULONG *dst, ULONG frames,
                           UBYTE *cs_left, UBYTE *cs_right, ULONG *frame_counter)
{
    ULONG i;
    ULONG fc = *frame_counter;

    for (i = 0; i < frames; i++) {
        WORD left = src[i * 2];
        WORD right = src[i * 2 + 1];
        UBYTE cs_bit_l = (cs_left[fc / 8] >> (fc % 8)) & 1;
        UBYTE cs_bit_r = (cs_right[fc / 8] >> (fc % 8)) & 1;
        UBYTE preamble_left = (fc == 0) ? 0x08 : 0x02;

        dst[i * 2]     = AROS_LONG2LE(encode_iec958_subframe(left, cs_bit_l, preamble_left));
        dst[i * 2 + 1] = AROS_LONG2LE(encode_iec958_subframe(right, cs_bit_r, 0x04));

        if (++fc >= 192)
            fc = 0;
    }

    *frame_counter = fc;
}


/******************************************************************************
** HDMI Audio InfoFrame *******************************************************
******************************************************************************/

static UBYTE srate_to_cea_sf(ULONG samplerate)
{
    switch (samplerate) {
    case 32000:  return 1;
    case 44100:  return 2;
    case 48000:  return 3;
    case 88200:  return 4;
    case 96000:  return 5;
    case 176400: return 6;
    case 192000: return 7;
    default:     return 0;
    }
}

static void hdmi_write_audio_infoframe(struct RPiHDMIData *dd)
{
    IPTR slot_base = REG_RAM_PKT_START(dd) + 4 * 0x24;
    UBYTE infoframe[14];
    UBYTE checksum;
    int i;

    infoframe[0] = 0x84; /* Audio InfoFrame type */
    infoframe[1] = 0x01; /* Version 1 */
    infoframe[2] = 0x0A; /* Length = 10 */
    infoframe[3] = 0x00; /* Checksum placeholder */
    infoframe[4] = 0x11; /* CC=1 (2ch), CT=1 (L-PCM) */
    infoframe[5] = (srate_to_cea_sf(dd->samplerate) << 2) | 0x01;
    for (i = 6; i < 14; i++)
        infoframe[i] = 0;

    checksum = 0;
    for (i = 0; i < 14; i++)
        if (i != 3)
            checksum += infoframe[i];
    infoframe[3] = (UBYTE)(0x100 - checksum);

    for (i = 0; i < 14; i += 4) {
        ULONG word = 0;
        int j;
        for (j = 0; j < 4 && (i + j) < 14; j++)
            word |= ((ULONG)infoframe[i + j]) << (j * 8);
        wr32le(slot_base + i, word);
    }

    /* Enable audio infoframe packet (bit 4 = packet_id 4) */
    wr32le(REG_RAM_PKT_CFG(dd), rd32le(REG_RAM_PKT_CFG(dd)) | (1 << 4));
}


/******************************************************************************
** HDMI MAI setup *************************************************************
******************************************************************************/

void hdmi_mai_init(struct RPiHDMIData *dd)
{
    IPTR pb = dd->periiobase;
    ULONG srate_enum = srate_to_mai_enum(dd->samplerate);
    ULONG n_value = srate_to_n(dd->samplerate);

    /* Reset MAI */
    wr32le(REG_MAI_CTL(dd), MAI_CTL_RESET);
    udelay(pb, 100);
    wr32le(REG_MAI_CTL(dd), MAI_CTL_ERRORF);
    wr32le(REG_MAI_CTL(dd), MAI_CTL_FLUSH);
    udelay(pb, 100);

    /* Audio format: PCM at configured sample rate */
    wr32le(REG_MAI_FMT(dd), MAI_FMT_FORMAT_PCM | MAI_FMT_RATE(srate_enum));

    /* FIFO thresholds */
    wr32le(REG_MAI_THR(dd),
           MAI_THR_DREQL(0x10) | MAI_THR_DREQH(0x10) |
           MAI_THR_PANICL(0x10) | MAI_THR_PANICH(0x10));

    /* MAI_CONFIG: BIT_REVERSE | FORMAT_REVERSE | stereo channel mask */
    wr32le(REG_MAI_CONFIG(dd),
           MAI_CONFIG_BIT_REVERSE | MAI_CONFIG_FORMAT_REVERSE |
           MAI_CONFIG_CHANNEL_MASK(0x03));

    /* Channel map for stereo */
    wr32le(REG_MAI_CHANNEL_MAP(dd), 0x08);

    /* Audio packet config */
    wr32le(REG_AUDIO_PKT_CFG(dd),
           AUDIO_PKT_ZERO_DATA_ON_FLAT | AUDIO_PKT_ZERO_DATA_ON_INACTIVE |
           AUDIO_PKT_B_FRAME_ID(0x8) | AUDIO_PKT_CEA_MASK(0x03));

    /* Sample rate clock divider: N = hsm_clock / samplerate */
    wr32le(REG_MAI_SMP(dd), (216000000 / dd->samplerate) << 8);

    /* CTS/N audio clock recovery */
    wr32le(REG_CRP_CFG(dd), CRP_CFG_EXTERNAL_CTS_EN | CRP_CFG_N(n_value));

    {
        ULONG cts = rd32le(REG_CTS_0(dd));
        if (cts == 0)
            cts = (148500UL * n_value) / (128 * (dd->samplerate / 1000));
        wr32le(REG_CTS_0(dd), cts);
        wr32le(REG_CTS_1(dd), cts);
    }

    /* Write Audio InfoFrame */
    hdmi_write_audio_infoframe(dd);

    /* Enable MAI */
    wr32le(REG_MAI_CTL(dd),
           MAI_CTL_CHALIGN | MAI_CTL_WHOLSMP | MAI_CTL_CHNUM(2) | MAI_CTL_ENABLE);

    /* Initialize IEC958 channel status */
    spdif_setup_channel_status(dd->channel_status_l, dd->channel_status_r, dd->samplerate);
    dd->frame_counter = 0;
}

void hdmi_mai_stop(struct RPiHDMIData *dd)
{
    wr32le(REG_MAI_CTL(dd), MAI_CTL_FLUSH | MAI_CTL_DLATE | MAI_CTL_ERRORE | MAI_CTL_ERRORF);
    udelay(dd->periiobase, 100);
}


/******************************************************************************
** DMA setup ******************************************************************
******************************************************************************/

void dma_build_control_blocks(struct RPiHDMIData *dd)
{
    ULONG mai_data_bus = (dd->variant == VARIANT_BCM2711)
        ? HDMI_MAI_DATA_BUS_BCM2711
        : HDMI_MAI_DATA_BUS_BCM2835;
    int i;

    for (i = 0; i < 2; i++) {
        struct DMAControlBlock *cb = dd->cb[i];

        cb->ti = DMA_TI_INTEN | DMA_TI_WAIT_RESP | DMA_TI_DEST_DREQ |
                 DMA_TI_SRC_INC | DMA_TI_BURST_LENGTH(2) |
                 DMA_TI_PERMAP(DMA_DREQ_HDMI) | DMA_TI_NO_WIDE_BURSTS;

        cb->source_ad = gpu_bus_addr((IPTR)dd->dmabuf[i]);
        cb->dest_ad   = mai_data_bus;
        cb->txfr_len  = dd->dmabuf_size;
        cb->stride    = 0;
        cb->nextconbk = gpu_bus_addr((IPTR)dd->cb[1 - i]);
        cb->reserved[0] = 0;
        cb->reserved[1] = 0;
    }
}

void dma_setup(struct RPiHDMIData *dd, ULONG cb_bus_addr)
{
    IPTR dma_base = dd->periiobase + 0x007000 + dd->dma_channel * 0x100;
    IPTR enable_addr = dd->periiobase + 0x007FF0;

    wr32le(enable_addr, rd32le(enable_addr) | (1 << dd->dma_channel));
    wr32le(dma_base + 0x00, DMA_CS_RESET);
    udelay(dd->periiobase, 10);
    wr32le(dma_base + 0x00, DMA_CS_INT | DMA_CS_END);
    wr32le(dma_base + 0x04, cb_bus_addr);
    wr32le(dma_base + 0x00,
           DMA_CS_WAIT_FOR_WRITES | DMA_CS_PANIC_PRI(15) |
           DMA_CS_PRI(8) | DMA_CS_ACTIVE);
    udelay(dd->periiobase, 10);
}

void dma_stop(struct RPiHDMIData *dd)
{
    IPTR dma_base = dd->periiobase + 0x007000 + dd->dma_channel * 0x100;

    wr32le(dma_base + 0x00, 0);
    udelay(dd->periiobase, 50);
    wr32le(dma_base + 0x00, DMA_CS_RESET);
    udelay(dd->periiobase, 100);
    wr32le(dma_base + 0x04, 0);
    wr32le(dma_base + 0x00, DMA_CS_INT | DMA_CS_END);
}
