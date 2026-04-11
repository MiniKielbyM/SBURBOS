#include ".h/SBURB_fontlib.h"

char scancode_map[128] = {
    0, 27, '1', '2', '3', '4', '5', '6', '7', '8', '9', '0', '-', '=', '\b',
    '\t', 'q', 'w', 'e', 'r', 't', 'y', 'u', 'i', 'o', 'p', '[', ']', '\n',
    0, 'a', 's', 'd', 'f', 'g', 'h', 'j', 'k', 'l', ';', '\'', '`',
    0, '\\', 'z', 'x', 'c', 'v', 'b', 'n', 'm', ',', '.', '/',
    0, '*', 0, ' '};

PSF1_Header *psf = (PSF1_Header *)font_psf;
uint8_t *glyphs = (uint8_t *)font_psf + sizeof(PSF1_Header);

uint8_t *psf_get_glyph(uint8_t c)
{
    return glyphs + (c * psf->charsize);
}

uint8_t psf_get_height(void)
{
    return psf->charsize;
}

// PSF text rendering
void print(char string[])
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

void println(char string[])
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

void delete_last_char()
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