/*
 * Copyright (C) 2026, The AROS Development Team. All rights reserved.
 * Author: Fabian Schmieder
 */
/*
 * battclock.resource for Raspberry Pi 4 — DS3231 RTC via I2C
 *
 * Reads/writes time from a DS3231 or compatible I2C RTC module
 * connected to the RPi4 I2C1 bus (GPIO 2/3, address 0x68).
 *
 * If no RTC is detected, ReadBattClock returns 0.
 */

#include <aros/debug.h>
#include <aros/symbolsets.h>
#include <aros/libcall.h>
#include <exec/types.h>
#include <proto/exec.h>
#include <proto/kernel.h>
#include <hidd/i2c.h>

#include "battclock_intern.h"

#include LC_LIBDEFS_FILE

/* BCD to binary conversion */
#define BCD2BIN(x) (((x) >> 4) * 10 + ((x) & 0x0F))
#define BIN2BCD(x) ((((x) / 10) << 4) | ((x) % 10))

/* Days per month (non-leap) */
static const UBYTE days_in_month[] = {
    31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31
};

static BOOL is_leap_year(ULONG year)
{
    return (year % 4 == 0 && (year % 100 != 0 || year % 400 == 0));
}

/*
 * Convert date/time to Amiga time (seconds since 1978-01-01 00:00:00).
 */
static ULONG datetime_to_amiga(ULONG year, ULONG month, ULONG day,
                                ULONG hour, ULONG min, ULONG sec)
{
    ULONG days = 0;
    ULONG y, m;

    for (y = AMIGA_EPOCH_YEAR; y < year; y++) {
        days += is_leap_year(y) ? 366 : 365;
    }

    for (m = 1; m < month; m++) {
        days += days_in_month[m - 1];
        if (m == 2 && is_leap_year(year))
            days++;
    }

    days += day - 1;

    return days * 86400 + hour * 3600 + min * 60 + sec;
}

/*
 * Convert Amiga time to date/time components.
 */
static void amiga_to_datetime(ULONG time, ULONG *year, ULONG *month,
                              ULONG *day, ULONG *hour, ULONG *min, ULONG *sec)
{
    ULONG days = time / 86400;
    ULONG rem = time % 86400;
    ULONG y, m, dpm;

    *hour = rem / 3600;
    rem %= 3600;
    *min = rem / 60;
    *sec = rem % 60;

    y = AMIGA_EPOCH_YEAR;
    while (1) {
        ULONG yd = is_leap_year(y) ? 366 : 365;
        if (days < yd)
            break;
        days -= yd;
        y++;
    }
    *year = y;

    for (m = 1; m <= 12; m++) {
        dpm = days_in_month[m - 1];
        if (m == 2 && is_leap_year(y))
            dpm++;
        if (days < dpm)
            break;
        days -= dpm;
    }
    *month = m;
    *day = days + 1;
}

/*
 * I2C read: write register address, then read bytes.
 * Uses direct BSC1 register access (same as our I2C HIDD).
 */
#define PERIBASE KrnGetSystemAttr(KATTR_PeripheralBase)
#define BSC1_BASE   (PERIBASE + 0x804000)
#define BSC_CONTROL 0x00
#define BSC_STATUS  0x04
#define BSC_DATALEN 0x08
#define BSC_SLAVE   0x0C
#define BSC_FIFO    0x10

#define BSC_CTL_I2CEN (1 << 15)
#define BSC_CTL_START (1 << 7)
#define BSC_CTL_READ  (1 << 0)
#define BSC_CTL_CLEAR (3 << 4)
#define BSC_STA_DONE  (1 << 1)
#define BSC_CLEAR_ALL (BSC_STA_DONE | (1 << 8) | (1 << 9))

static inline ULONG bsc_rd(ULONG off)
{
    return AROS_LE2LONG(*(volatile ULONG *)(BSC1_BASE + off));
}

static inline void bsc_wr(ULONG off, ULONG val)
{
    *(volatile ULONG *)(BSC1_BASE + off) = AROS_LONG2LE(val);
}

static void bsc_wait(void)
{
    while (!(bsc_rd(BSC_STATUS) & BSC_STA_DONE))
        ;
}

static BOOL rtc_read(UBYTE reg, UBYTE *buf, ULONG len)
{
    ULONG i;

    /* Write register address */
    bsc_wr(BSC_SLAVE, DS3231_ADDR);
    bsc_wr(BSC_DATALEN, 1);
    bsc_wr(BSC_STATUS, BSC_CLEAR_ALL);
    bsc_wr(BSC_FIFO, reg);
    bsc_wr(BSC_CONTROL, BSC_CTL_I2CEN | BSC_CTL_START);
    bsc_wait();

    /* Read data */
    bsc_wr(BSC_DATALEN, len);
    bsc_wr(BSC_STATUS, BSC_CLEAR_ALL);
    bsc_wr(BSC_CONTROL, BSC_CTL_I2CEN | BSC_CTL_START | BSC_CTL_READ);
    bsc_wait();

    for (i = 0; i < len; i++)
        buf[i] = (UBYTE)bsc_rd(BSC_FIFO);

    return TRUE;
}

static BOOL rtc_write(UBYTE reg, UBYTE *buf, ULONG len)
{
    ULONG i;

    bsc_wr(BSC_SLAVE, DS3231_ADDR);
    bsc_wr(BSC_DATALEN, 1 + len);
    bsc_wr(BSC_STATUS, BSC_CLEAR_ALL);
    bsc_wr(BSC_FIFO, reg);
    for (i = 0; i < len; i++)
        bsc_wr(BSC_FIFO, buf[i]);
    bsc_wr(BSC_CONTROL, BSC_CTL_I2CEN | BSC_CTL_START);
    bsc_wait();

    return TRUE;
}

/* ============================================================ */

static int BattClock_Init(struct BattClockBase *base)
{
    InitSemaphore(&base->bb_Sem);
    D(bug("[BattClock] DS3231 RTC on I2C1 (0x%02x)\n", DS3231_ADDR));
    return TRUE;
}

AROS_LH0(void, ResetBattClock,
    struct BattClockBase *, BattClockBase, 1, Battclock)
{
    AROS_LIBFUNC_INIT
    /* Nothing to reset on DS3231 */
    AROS_LIBFUNC_EXIT
}

AROS_LH0(ULONG, ReadBattClock,
    struct BattClockBase *, BattClockBase, 2, Battclock)
{
    AROS_LIBFUNC_INIT

    UBYTE regs[7];
    ULONG year, month, day, hour, min, sec;

    ObtainSemaphore(&BattClockBase->bb_Sem);

    if (!rtc_read(DS3231_SECONDS, regs, 7)) {
        ReleaseSemaphore(&BattClockBase->bb_Sem);
        return 0;
    }

    ReleaseSemaphore(&BattClockBase->bb_Sem);

    sec   = BCD2BIN(regs[0] & 0x7F);
    min   = BCD2BIN(regs[1] & 0x7F);
    hour  = BCD2BIN(regs[2] & 0x3F);
    day   = BCD2BIN(regs[4] & 0x3F);
    month = BCD2BIN(regs[5] & 0x1F);
    year  = BCD2BIN(regs[6]) + 2000;

    if (regs[5] & 0x80) /* Century bit */
        year += 100;

    D(bug("[BattClock] Read: %04ld-%02ld-%02ld %02ld:%02ld:%02ld\n",
          year, month, day, hour, min, sec));

    return datetime_to_amiga(year, month, day, hour, min, sec);

    AROS_LIBFUNC_EXIT
}

AROS_LH1(void, WriteBattClock,
    AROS_LHA(ULONG, time, D0),
    struct BattClockBase *, BattClockBase, 3, Battclock)
{
    AROS_LIBFUNC_INIT

    ULONG year, month, day, hour, min, sec;
    UBYTE regs[7];

    amiga_to_datetime(time, &year, &month, &day, &hour, &min, &sec);

    regs[0] = BIN2BCD(sec);
    regs[1] = BIN2BCD(min);
    regs[2] = BIN2BCD(hour);
    regs[3] = 0; /* Day of week — not used */
    regs[4] = BIN2BCD(day);
    regs[5] = BIN2BCD(month);
    regs[6] = BIN2BCD(year % 100);

    if (year >= 2100)
        regs[5] |= 0x80; /* Century bit */

    ObtainSemaphore(&BattClockBase->bb_Sem);
    rtc_write(DS3231_SECONDS, regs, 7);
    ReleaseSemaphore(&BattClockBase->bb_Sem);

    D(bug("[BattClock] Write: %04ld-%02ld-%02ld %02ld:%02ld:%02ld\n",
          year, month, day, hour, min, sec));

    AROS_LIBFUNC_EXIT
}

ADD2INITLIB(BattClock_Init, 0)
