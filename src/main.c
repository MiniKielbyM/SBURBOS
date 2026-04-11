#include "includefile.h"

#define LOGO_WIDTH 960
#define LOGO_HEIGHT 1017

Image logo = {
    .width = LOGO_WIDTH,
    .height = LOGO_HEIGHT,
    .data = (uint32_t *)output_rgba};


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
    println(concat(buf, "Year: ", itoa(current_time.year, year_str, 10)));
    println(concat(buf, "Month: ", itoa(current_time.month, month_str, 10)));
    println(concat(buf, "Day: ", itoa(current_time.day, day_str, 10)));
    println(concat(buf, "Hour: ", itoa(current_time.hour, hour_str, 10)));
    println(concat(buf, "Minute: ", itoa(current_time.minute, minute_str, 10)));
    println(concat(buf, "Second: ", itoa(current_time.second, second_str, 10)));
    while (1)
    {
        __asm__ volatile("hlt");
    }
    println("[SBURBOS] main loop exited, halting");
    hcf();
}