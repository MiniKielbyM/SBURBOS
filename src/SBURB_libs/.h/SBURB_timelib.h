#ifndef SBURB_TIMELIB_H
#define SBURB_TIMELIB_H
#include <stdint.h>
#include "SBURB_portlib.h"

#define PIT_FREQ 1193182
#define CMOS_ADDR 0x70
#define CMOS_DATA 0x71

typedef struct
{
    uint8_t second;
    uint8_t minute;
    uint8_t hour;
    uint8_t weekday;
    uint8_t day;
    uint8_t month;
    uint16_t year;
} rtc_time_t;

extern volatile uint8_t tick;
extern rtc_time_t current_time;
extern uint64_t cpu_hz;

uint64_t get_cpu_hz();
uint64_t rdtsc();
uint8_t cmos_read(uint8_t reg);
int cmos_updating(void);
uint8_t bcd_to_bin(uint8_t bcd);
void rtc_read(rtc_time_t *t);
void pit_init(uint32_t frequency);
void sleep(uint64_t ms);

#endif // SBURB_TIMELIB_H