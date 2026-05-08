#ifndef BATTCLOCK_INTERN_H
 * Author: Fabian Schmieder
#define BATTCLOCK_INTERN_H

#include <exec/types.h>
#include <exec/libraries.h>
#include <exec/semaphores.h>

struct BattClockBase {
    struct Library  bb_LibNode;
    struct SignalSemaphore bb_Sem;
};

/* DS3231 I2C address */
#define DS3231_ADDR     0x68

/* DS3231 register offsets */
#define DS3231_SECONDS  0x00
#define DS3231_MINUTES  0x01
#define DS3231_HOURS    0x02
#define DS3231_DAY      0x03
#define DS3231_DATE     0x04
#define DS3231_MONTH    0x05
#define DS3231_YEAR     0x06

/* Amiga epoch: 1978-01-01 00:00:00 UTC */
#define AMIGA_EPOCH_YEAR 1978

#endif /* BATTCLOCK_INTERN_H */
