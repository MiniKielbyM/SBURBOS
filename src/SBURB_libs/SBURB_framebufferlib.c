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
