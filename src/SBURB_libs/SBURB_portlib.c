#include ".h/SBURB_portlib.h"



void outb(uint16_t port, uint8_t value)
{
    asm volatile("outb %0, %1" : : "a"(value), "Nd"(port));
}

uint8_t inb(uint16_t port)
{
    uint8_t ret;
    __asm__ volatile("inb %1, %0" : "=a"(ret) : "Nd"(port));
    return ret;
}

void pic_remap(void)
{
    // ICW1: Start initialization sequence (cascade mode, ICW4 needed)
    outb(0x20, 0x11); // master command port
    outb(0xA0, 0x11); // slave command port

    // ICW2: Set base interrupt vector
    outb(0x21, 0x20); // master: IRQs 0-7  -> vectors 32-39
    outb(0xA1, 0x28); // slave:  IRQs 8-15 -> vectors 40-47

    // ICW3: Configure cascade (how master/slave are wired)
    outb(0x21, 0x04); // master: slave is on IRQ 2
    outb(0xA1, 0x02); // slave:  connected to master's IRQ 2

    // ICW4: Set 8086 mode
    outb(0x21, 0x01);
    outb(0xA1, 0x01);

    // Set interrupt masks (1 = masked/disabled, 0 = enabled)
    outb(0x21, 0xFC); // master: enable IRQ 0 (timer) and IRQ 1 (keyboard)
    outb(0xA1, 0xFF); // slave:  mask everything
}