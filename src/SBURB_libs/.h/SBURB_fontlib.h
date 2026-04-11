#ifndef SBURB_FONTLIB_H
#define SBURB_FONTLIB_H

#include <stdint.h>
#include "SBURB_framebufferlib.h"
#include "SBURB_globals.h"

typedef struct
{
    uint8_t magic[2]; // 0x36 0x04 for PSF1
    uint8_t mode;
    uint8_t charsize; // bytes per glyph
} PSF1_Header;

extern unsigned char font_psf[];
extern unsigned int Cyr_a8x14_psf_len;
extern PSF1_Header *psf;
extern uint8_t *glyphs;
extern unsigned int Cyr_a8x14_psf_len;
extern char scancode_map[128];

uint8_t *psf_get_glyph(uint8_t c);
uint8_t psf_get_height(void);
void print(char string[]);
void println(char string[]);
void delete_last_char();

#endif // SBURB_FONTLIB_H