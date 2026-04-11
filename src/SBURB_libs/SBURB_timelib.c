#include ".h/SBURB_timelib.h"

volatile uint8_t tick = 0;
rtc_time_t current_time;
uint64_t cpu_hz;

uint64_t rdtsc()
{
    uint32_t lo, hi;
    __asm__ volatile("rdtsc" : "=a"(lo), "=d"(hi));
    return ((uint64_t)hi << 32) | lo;
}

uint64_t get_cpu_hz()
{
    uint16_t divisor = 0xFFFF; // max interval (~54.9 ms)

    // Set PIT channel 0: mode 0 (one-shot), lobyte/hibyte
    outb(0x43, 0x30);
    outb(0x40, divisor & 0xFF);
    outb(0x40, divisor >> 8);

    uint64_t start = rdtsc();

    // Wait until PIT reaches zero
    // Poll using latch command
    while (1)
    {
        outb(0x43, 0x00); // latch count
        uint8_t lo = inb(0x40);
        uint8_t hi = inb(0x40);
        uint16_t count = (hi << 8) | lo;

        if (count == 0)
            break;
    }

    uint64_t end = rdtsc();

    uint64_t tsc_delta = end - start;

    // Time elapsed = divisor / PIT_FREQ seconds
    // CPU Hz = tsc_delta / time
    return (tsc_delta * PIT_FREQ) / divisor;
}

uint8_t cmos_read(uint8_t reg)
{
    outb(CMOS_ADDR, reg);
    return inb(CMOS_DATA);
}

int cmos_updating(void)
{
    outb(CMOS_ADDR, 0x0A);
    return inb(CMOS_DATA) & 0x80;
}

uint8_t bcd_to_bin(uint8_t bcd)
{
    return ((bcd >> 4) * 10) + (bcd & 0x0F);
}

void rtc_read(rtc_time_t *t)
{
    while (cmos_updating())
        ;

    t->second = cmos_read(0x00);
    t->minute = cmos_read(0x02);
    t->hour = cmos_read(0x04);
    t->weekday = cmos_read(0x06);
    t->day = cmos_read(0x07);
    t->month = cmos_read(0x08);
    t->year = cmos_read(0x09);

    uint8_t status_b = cmos_read(0x0B);

    // Convert BCD to binary if needed
    if (!(status_b & 0x04))
    {
        t->second = bcd_to_bin(t->second);
        t->minute = bcd_to_bin(t->minute);
        t->hour = bcd_to_bin(t->hour & 0x7F) | (t->hour & 0x80);
        t->day = bcd_to_bin(t->day);
        t->month = bcd_to_bin(t->month);
        t->year = bcd_to_bin(t->year);
    }

    t->year += 2000;
}

void pit_init(uint32_t frequency_hz)
{
    uint16_t divisor = 1193180 / frequency_hz; // PIT base clock is ~1.193 MHz
    outb(0x43, 0x36);                          // channel 0, lobyte/hibyte, square wave
    outb(0x40, divisor & 0xFF);                // low byte
    outb(0x40, (divisor >> 8) & 0xFF);         // high byte
}

void sleep(uint64_t ms)
{
    uint64_t start = rdtsc();
    uint64_t target = (cpu_hz * ms) / 1000;

    while (rdtsc() - start < target)
        ;
}