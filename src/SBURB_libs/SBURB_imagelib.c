#include ".h/SBURB_imagelib.h"

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
            put_pixel(row, pitch, x + x_off, 0, color);
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
            put_pixel(row, pitch, x, y, img->data[src_y * img->width + src_x]);
        }
    }
}
