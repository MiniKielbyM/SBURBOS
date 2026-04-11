#include ".h/SBURB_seriallib.h"
// Serial
void serial_init(void)
{
    outb(0x3F8 + 1, 0x00);
    outb(0x3F8 + 3, 0x80);
    outb(0x3F8 + 0, 0x01);
    outb(0x3F8 + 1, 0x00);
    outb(0x3F8 + 3, 0x03);
    outb(0x3F8 + 2, 0xC7);
    outb(0x3F8 + 4, 0x0B);
}

void serial_write_char(char c)
{
    while ((inb(0x3F8 + 5) & 0x20) == 0)
    {
    }
    outb(0x3F8, (uint8_t)c);
}

void serial_write(const char *s)
{
    while (*s)
    {
        if (*s == '\n')
            serial_write_char('\r');
        serial_write_char(*s++);
    }
}

void hcf(void)
{
    for (;;)
        asm("hlt");
}