#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <limine.h>
#include <string.h>
#include "font.h"
#include "image.h"

#define LOGO_WIDTH 960
#define LOGO_HEIGHT 1017
#define IDT_ENTRIES 256
#define PIT_FREQ 1193182
#define SBURBOS_VERSION "DEV-0.0.0"
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

typedef struct
{
    uint8_t magic[2]; // 0x36 0x04 for PSF1
    uint8_t mode;
    uint8_t charsize; // bytes per glyph
} PSF1_Header;

typedef struct
{
    uint32_t width;
    uint32_t height;
    uint32_t *data; // 0xAARRGGBB
} Image;

// IDT entry structure
struct idt_entry
{
    uint16_t offset_low;
    uint16_t selector;
    uint8_t ist;
    uint8_t type_attr;
    uint16_t offset_mid;
    uint32_t offset_high;
    uint32_t reserved;
} __attribute__((packed));

// IDT pointer structure
struct idt_ptr
{
    uint16_t limit;
    uint64_t base;
} __attribute__((packed));

Image logo = {
    .width = LOGO_WIDTH,
    .height = LOGO_HEIGHT,
    .data = (uint32_t *)output_rgba};

rtc_time_t current_time;

static struct idt_entry idt[IDT_ENTRIES];
static struct idt_ptr idtp;

// external IRQ stubs (from irq_stubs.S)
extern void irq0();
extern void irq1();

char scancode_map[128] = {
    0, 27, '1', '2', '3', '4', '5', '6', '7', '8', '9', '0', '-', '=', '\b',
    '\t', 'q', 'w', 'e', 'r', 't', 'y', 'u', 'i', 'o', 'p', '[', ']', '\n',
    0, 'a', 's', 'd', 'f', 'g', 'h', 'j', 'k', 'l', ';', '\'', '`',
    0, '\\', 'z', 'x', 'c', 'v', 'b', 'n', 'm', ',', '.', '/',
    0, '*', 0, ' '};

// internal pointers
static PSF1_Header *psf = (PSF1_Header *)font_psf;
static uint8_t *glyphs = (uint8_t *)font_psf + sizeof(PSF1_Header);

uint8_t *psf_get_glyph(uint8_t c)
{
    return glyphs + (c * psf->charsize);
}

uint8_t psf_get_height(void)
{
    return psf->charsize;
}
// global text position

uint64_t cpu_hz;

int x = 10, y = 10, num_lines = 0;

int line_lengths[1024];

volatile uint8_t tick = 0;

// Limine base revision
__attribute__((used, section(".limine_requests"))) static volatile uint64_t limine_base_revision[] = LIMINE_BASE_REVISION(3);

// Framebuffer request
__attribute__((used, section(".limine_requests"))) static volatile struct limine_framebuffer_request framebuffer_request = {
    .id = LIMINE_FRAMEBUFFER_REQUEST_ID,
    .revision = 0};

// Limine markers
__attribute__((used, section(".limine_requests_start"))) static volatile uint64_t limine_requests_start_marker[] = LIMINE_REQUESTS_START_MARKER;

__attribute__((used, section(".limine_requests_end"))) static volatile uint64_t limine_requests_end_marker[] = LIMINE_REQUESTS_END_MARKER;

// libc replacements
void *memcpy(void *restrict dest, const void *restrict src, size_t n)
{
    uint8_t *d = dest;
    const uint8_t *s = src;
    for (size_t i = 0; i < n; i++)
        d[i] = s[i];
    return dest;
}

void *memset(void *s, int c, size_t n)
{
    uint8_t *p = s;
    for (size_t i = 0; i < n; i++)
        p[i] = (uint8_t)c;
    return s;
}

void *memmove(void *dest, const void *src, size_t n)
{
    uint8_t *d = dest;
    const uint8_t *s = src;

    if (s > d)
    {
        for (size_t i = 0; i < n; i++)
            d[i] = s[i];
    }
    else
    {
        for (size_t i = n; i > 0; i--)
            d[i - 1] = s[i - 1];
    }

    return dest;
}

int memcmp(const void *s1, const void *s2, size_t n)
{
    const uint8_t *a = s1;
    const uint8_t *b = s2;

    for (size_t i = 0; i < n; i++)
    {
        if (a[i] != b[i])
            return a[i] < b[i] ? -1 : 1;
    }
    return 0;
}

// I/O
static inline void outb(uint16_t port, uint8_t value)
{
    asm volatile("outb %0, %1" : : "a"(value), "Nd"(port));
}

static inline uint8_t inb(uint16_t port)
{
    uint8_t ret;
    __asm__ volatile("inb %1, %0" : "=a"(ret) : "Nd"(port));
    return ret;
}

// Serial
static void serial_init(void)
{
    outb(0x3F8 + 1, 0x00);
    outb(0x3F8 + 3, 0x80);
    outb(0x3F8 + 0, 0x01);
    outb(0x3F8 + 1, 0x00);
    outb(0x3F8 + 3, 0x03);
    outb(0x3F8 + 2, 0xC7);
    outb(0x3F8 + 4, 0x0B);
}

static void serial_write_char(char c)
{
    while ((inb(0x3F8 + 5) & 0x20) == 0)
    {
    }
    outb(0x3F8, (uint8_t)c);
}

static void serial_write(const char *s)
{
    while (*s)
    {
        if (*s == '\n')
            serial_write_char('\r');
        serial_write_char(*s++);
    }
}

// Halt
static void hcf(void)
{
    for (;;)
        asm("hlt");
}

// PSF text rendering
static void print(char string[])
{
    struct limine_framebuffer *framebuffer =
        framebuffer_request.response->framebuffers[0];

    uint32_t *fb_ptr = (uint32_t *)framebuffer->address;
    uint32_t pitch = framebuffer->pitch / sizeof(uint32_t);
    uint32_t white = 0xFFFFFFFF;

    for (int i = 0; string[i] != '\0'; i++)
    {
        if (string[i] == '\n')
        {
            x = 10;
            y += 16;
            num_lines++;
            continue;
        }
        uint8_t *glyph = psf_get_glyph((uint8_t)string[i]);
        line_lengths[num_lines] += 8;
        for (int row = 0; row < psf_get_height(); row++)
        {
            uint8_t byte = glyph[row];

            for (int col = 0; col < 8; col++)
            {
                if (byte & (0x80 >> col))
                {
                    fb_ptr[(y + row) * pitch + (x + col)] = white;
                }
            }
        }

        x += 8;
    }
}

static void println(char string[])
{
    struct limine_framebuffer *framebuffer =
        framebuffer_request.response->framebuffers[0];

    uint32_t *fb_ptr = (uint32_t *)framebuffer->address;
    uint32_t pitch = framebuffer->pitch / sizeof(uint32_t);
    uint32_t white = 0xFFFFFFFF;

    for (int i = 0; string[i] != '\0'; i++)
    {
        if (string[i] == '\n')
        {
            x = 10;
            y += 16;
            num_lines++;
            continue;
        }

        uint8_t *glyph = psf_get_glyph((uint8_t)string[i]);
        line_lengths[num_lines] += 8;
        for (int row = 0; row < psf_get_height(); row++)
        {
            uint8_t byte = glyph[row];

            for (int col = 0; col < 8; col++)
            {
                if (byte & (0x80 >> col))
                {
                    fb_ptr[(y + row) * pitch + (x + col)] = white;
                }
            }
        }

        x += 8;
    }
    y += 16;
    x = 10;
    num_lines++;
}

static void delete_last_char()
{
    if (x > 10)
    {
        x -= 8;
        line_lengths[num_lines] -= 8;
    }
    else if (y > 10)
    {
        y -= 16;
        x = 10 + line_lengths[num_lines - 1];
        num_lines--;
    }
    else
    {
        return; // at the beginning of the screen, nothing to delete
    }

    struct limine_framebuffer *framebuffer =
        framebuffer_request.response->framebuffers[0];
    uint32_t *fb_ptr = (uint32_t *)framebuffer->address;
    uint32_t pitch = framebuffer->pitch / sizeof(uint32_t);

    for (int row = 0; row < 16; row++)
    {
        for (int col = 0; col < 8; col++)
        {
            fb_ptr[(y + row) * pitch + (x + col)] = 0; // clear to black
        }
    }
}

bool keyboard_has_data()
{
    return inb(0x64) & 1; // output buffer full
}

uint8_t keyboard_read()
{
    return inb(0x60);
}

static void clear_screen()
{
    struct limine_framebuffer *framebuffer =
        framebuffer_request.response->framebuffers[0];

    uint32_t *fb_ptr = (uint32_t *)framebuffer->address;
    uint32_t pitch = framebuffer->pitch / sizeof(uint32_t);

    for (uint32_t y = 0; y < framebuffer->height; y++)
    {
        for (uint32_t x = 0; x < framebuffer->width; x++)
        {
            fb_ptr[y * pitch + x] = 0; // clear to black
        }
    }
    y = 10;
    x = 10;
    for (int i = 0; i <= num_lines; i++)
    {
        line_lengths[i] = 0;
    }
}

static uint8_t cmos_read(uint8_t reg)
{
    outb(CMOS_ADDR, reg);
    return inb(CMOS_DATA);
}

static int cmos_updating(void)
{
    outb(CMOS_ADDR, 0x0A);
    return inb(CMOS_DATA) & 0x80;
}

static uint8_t bcd_to_bin(uint8_t bcd)
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

void put_pixel(uint32_t *fb, uint64_t pitch, int x, int y, uint32_t color)
{
    uint32_t *row = (uint32_t *)((uint8_t *)fb + y * pitch);
    row[x] = color;
}

void draw_image(uint32_t *fb, uint64_t pitch, Image *img, int x_off, int y_off)
{
    for (uint32_t y = 0; y < img->height; y++)
    {
        for (uint32_t x = 0; x < img->width; x++)
        {
            uint32_t color = img->data[y * img->width + x];

            uint32_t *row = (uint32_t *)((uint8_t *)fb + (y + y_off) * pitch);
            row[x + x_off] = color;
        }
    }
}

void draw_image_fit(uint32_t *fb, uint64_t pitch, Image *img, uint32_t fb_width, uint32_t fb_height)
{
    for (uint32_t y = 0; y < fb_height; y++)
    {
        // map framebuffer y to image y
        uint32_t src_y = y * img->height / fb_height;

        uint32_t *row = (uint32_t *)((uint8_t *)fb + y * pitch);

        for (uint32_t x = 0; x < fb_width; x++)
        {
            // map framebuffer x to image x
            uint32_t src_x = x * img->width / fb_width;

            row[x] = img->data[src_y * img->width + src_x];
        }
    }
}

void pic_remap(void)
{
    // ICW1: Start initialization sequence (cascade mode, ICW4 needed)
    outb(0x20, 0x11); // master command port
    outb(0xA0, 0x11); // slave command port

    // ICW2: Set base interrupt vector
    outb(0x21, 0x20); // master: IRQs 0-7  -> vectors 32-39
    outb(0xA1, 0x28); // slave:  IRQs 8-15 -> vectors 40-47

    // ICW3: Configure cascade (how master/slave are wired)
    outb(0x21, 0x04); // master: slave is on IRQ 2
    outb(0xA1, 0x02); // slave:  connected to master's IRQ 2

    // ICW4: Set 8086 mode
    outb(0x21, 0x01);
    outb(0xA1, 0x01);

    // Set interrupt masks (1 = masked/disabled, 0 = enabled)
    outb(0x21, 0xFC); // master: enable IRQ 0 (timer) and IRQ 1 (keyboard)
    outb(0xA1, 0xFF); // slave:  mask everything
}

void irq_handler(uint64_t *regs)
{
    uint64_t irq_num = regs[7];

    if (irq_num == 33)
    {
        uint8_t sc = inb(0x60); // always read it

        if (!(sc & 0x80)) // only act on make codes
        {
            char c = scancode_map[sc];
            if (c == '\b')
                delete_last_char();
            else if (c == '\t')
                print("    ");
            else if (c != 0)
                print((char[]){c, '\0'});
        }
    }
    if (irq_num == 32)
    {
        tick++;

        if (tick % 1000 == 0)
            rtc_read(&current_time);
    }
    if (irq_num >= 40)
        outb(0xA0, 0x20);
    outb(0x20, 0x20);
}

// helper to set an IDT gate
static void idt_set_gate(int n, void (*handler)())
{
    uint64_t addr = (uint64_t)handler;

    idt[n].offset_low = addr & 0xFFFF;
    idt[n].selector = 0x28; // Limine kernel code segment
    idt[n].ist = 0;
    idt[n].type_attr = 0x8E; // interrupt gate, present, ring 0
    idt[n].offset_mid = (addr >> 16) & 0xFFFF;
    idt[n].offset_high = (addr >> 32) & 0xFFFFFFFF;
    idt[n].reserved = 0;
}

void idt_init(void)
{
    // zero entire IDT
    for (int i = 0; i < IDT_ENTRIES; i++)
    {
        idt[i] = (struct idt_entry){0};
    }

    // IRQs (after PIC remap)
    idt_set_gate(32, irq0); // timer
    idt_set_gate(33, irq1); // keyboard

    // load IDT
    idtp.limit = sizeof(idt) - 1;
    idtp.base = (uint64_t)&idt;

    __asm__ volatile("lidt %0" : : "m"(idtp));
}

void pit_init(uint32_t frequency_hz)
{
    uint16_t divisor = 1193180 / frequency_hz; // PIT base clock is ~1.193 MHz
    outb(0x43, 0x36);                          // channel 0, lobyte/hibyte, square wave
    outb(0x40, divisor & 0xFF);                // low byte
    outb(0x40, (divisor >> 8) & 0xFF);         // high byte
}

static inline uint64_t rdtsc()
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

void sleep(uint64_t ms)
{
    uint64_t start = rdtsc();
    uint64_t target = (cpu_hz * ms) / 1000;

    while (rdtsc() - start < target)
        ;
}

void play_startup_animation(uint32_t *fb, uint64_t pitch, uint32_t fb_width, uint32_t fb_height, Image *img, int scale_percent)
{
    clear_screen();
    // compute scaled dimensions using integer math
    uint32_t scaled_width = fb_width * scale_percent / 100;
    uint32_t scaled_height = fb_height * scale_percent / 100;

    // offsets to center the scaled image
    int x_off = (fb_width - scaled_width) / 2;
    int y_off = (fb_height - scaled_height) / 2;

    for (uint32_t y = 0; y < scaled_height; y += 1)
    {
        uint32_t src_y = y * img->height / scaled_height;
        uint32_t *row = (uint32_t *)((uint8_t *)fb + (y + y_off) * pitch);
        for (uint32_t x = 0; x < scaled_width; x += 1)
        {
            uint32_t src_x = x * img->width / scaled_width;
            row[x + x_off] = img->data[src_y * img->width + src_x];
        }
        sleep(10); // small delay for animation
    }
}

char *itoa(int value, char *str, int base)
{
    char *ptr = str, *ptr1 = str, tmp_char;
    int tmp_value;

    if (base < 2 || base > 36)
    {
        *str = '\0';
        return str;
    }

    do
    {
        tmp_value = value;
        value /= base;
        *ptr++ = "0123456789abcdefghijklmnopqrstuvwxyz"[tmp_value - value * base];
    } while (value);

    // Apply negative sign for base 10
    if (tmp_value < 0 && base == 10)
        *ptr++ = '-';

    *ptr-- = '\0';

    // Reverse the string
    while (ptr1 < ptr)
    {
        tmp_char = *ptr;
        *ptr-- = *ptr1;
        *ptr1++ = tmp_char;
    }
    return str;
}

char *concat(char *dest, const char *str1, const char *str2)
{
    char *ptr = dest;

    while (*str1)
        *ptr++ = *str1++;

    while (*str2)
        *ptr++ = *str2++;

    *ptr = '\0';
    return dest;
}

// Entry point
void kmain(void)
{
    println(("[SBURBOS] booting SBURBOS version " SBURBOS_VERSION));
    println("[SBURBOS] kernel entered kmain()");
    serial_init();
    println("[SBURBOS] serial initialized");
    if (!LIMINE_BASE_REVISION_SUPPORTED(limine_base_revision))
    {
        serial_write("[SBURBOS] Limine base revision not supported\n");
        hcf();
    }
    println("[SBURBOS] Limine base revision supported");

    if (framebuffer_request.response == NULL ||
        framebuffer_request.response->framebuffer_count < 1)
    {
        serial_write("[SBURBOS] no framebuffer received\n");
        hcf();
    }
    struct limine_framebuffer *fb = framebuffer_request.response->framebuffers[0];
    uint32_t *framebuffer = fb->address;
    uint64_t pitch = fb->pitch;
    println("[SBURBOS] framebuffer received");
    pic_remap();
    println("[SBURBOS] PIC remapped");
    idt_init(); // set up IDT and load with lidt
    println("[SBURBOS] IDT initialized");
    cpu_hz = get_cpu_hz();
    pit_init(1000); // restore periodic PIT AFTER measurement
    println("[SBURBOS] PIT initialized");
    println("[SBURBOS] starting sleep test for 1000 ms...");
    sleep(1000);
    println("[SBURBOS] sleep test complete");
    __asm__ volatile("sti");
    println("[SBURBOS] interrupts enabled");
    println("[SBURBOS] starting up...");
    sleep(2000);
    __asm__ volatile("cli");
    play_startup_animation(framebuffer, pitch, fb->width, fb->height, &logo, 50);
    __asm__ volatile("sti");
    rtc_read(&current_time);
    println("[SBURBOS] current time read from RTC:");
    char year_str[12];
    char month_str[12];
    char day_str[12];
    char hour_str[12];
    char minute_str[12];
    char second_str[12];
    char buf[64];
    println(concat(buf, "  Year: ", itoa(current_time.year, year_str, 10)));
    println(concat(buf, "  Month: ", itoa(current_time.month, month_str, 10)));
    println(concat(buf, "  Day: ", itoa(current_time.day, day_str, 10)));
    println(concat(buf, "  Hour: ", itoa(current_time.hour, hour_str, 10)));
    println(concat(buf, "  Minute: ", itoa(current_time.minute, minute_str, 10)));
    println(concat(buf, "  Second: ", itoa(current_time.second, second_str, 10)));
    sleep(5000);
    println(concat(buf, "  Year: ", itoa(current_time.year, year_str, 10)));
    println(concat(buf, "  Month: ", itoa(current_time.month, month_str, 10)));
    println(concat(buf, "  Day: ", itoa(current_time.day, day_str, 10)));
    println(concat(buf, "  Hour: ", itoa(current_time.hour, hour_str, 10)));
    println(concat(buf, "  Minute: ", itoa(current_time.minute, minute_str, 10)));
    println(concat(buf, "  Second: ", itoa(current_time.second, second_str, 10)));
    while (1)
    {
        __asm__ volatile("hlt");
    }
    println("[SBURBOS] main loop exited, halting");
    hcf();
}