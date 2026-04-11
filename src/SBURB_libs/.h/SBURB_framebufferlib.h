#ifndef SBURB_FRAMEBUFFERLIB_H
#define SBURB_FRAMEBUFFERLIB_H

#include "limine.h"

extern volatile struct limine_framebuffer_request framebuffer_request;
extern volatile uint64_t limine_base_revision[];
extern volatile uint64_t limine_requests_start_marker[];
extern volatile uint64_t limine_requests_end_marker[];

#endif // SBURB_FRAMEBUFFERLIB_H