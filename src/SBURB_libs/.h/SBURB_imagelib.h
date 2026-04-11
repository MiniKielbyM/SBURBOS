#ifndef SBURB_IMAGELIB_H
#define SBURB_IMAGELIB_H
#include <stdint.h>
#include "SBURB_framebufferlib.h"

typedef struct
{
    uint32_t width;
    uint32_t height;
    uint32_t *data; // 0xAARRGGBB
} Image;

void put_pixel(uint32_t *fb, uint64_t pitch, int x, int y, uint32_t color);
void draw_image(uint32_t *fb, uint64_t pitch, Image *img, int x_off, int y_off);
void draw_image_fit(uint32_t *fb, uint64_t pitch, Image *img, uint32_t fb_width, uint32_t fb_height);

#endif