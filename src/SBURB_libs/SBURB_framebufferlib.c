#include ".h/SBURB_framebufferlib.h"

// Limine base revision
__attribute__((used, section(".limine_requests"))) volatile uint64_t limine_base_revision[] = LIMINE_BASE_REVISION(3);

// Framebuffer request
__attribute__((used, section(".limine_requests"))) volatile struct limine_framebuffer_request framebuffer_request = {
    .id = LIMINE_FRAMEBUFFER_REQUEST_ID,
    .revision = 0};

// Limine markers
__attribute__((used, section(".limine_requests_start"))) volatile uint64_t limine_requests_start_marker[] = LIMINE_REQUESTS_START_MARKER;

__attribute__((used, section(".limine_requests_end"))) volatile uint64_t limine_requests_end_marker[] = LIMINE_REQUESTS_END_MARKER;

void clear_screen()
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