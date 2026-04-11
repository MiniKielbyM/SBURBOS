#ifndef SBURB_PORTLIB_H
#define SBURB_PORTLIB_H
#include <stdint.h>
#include <stddef.h>

void outb(uint16_t port, uint8_t value);
uint8_t inb(uint16_t port);
void pic_remap(void);

#endif // SBURB_PORTLIB_H
