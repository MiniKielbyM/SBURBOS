#include ".h/SBURB_interruptlib.h"

void irq_handler(uint64_t *regs)
{
    uint64_t irq_num = regs[7];

    if (irq_num == 33)
    {
        uint8_t sc = inb(0x60); // always read it

        if (!(sc & 0x80)) // only act on make codes
        {
            char c = scancode_map[sc];
            if (c == '\b')
                delete_last_char();
            else if (c == '\t')
                print("    ");
            else if (c != 0)
                print((char[]){c, '\0'});
        }
    }
    if (irq_num == 32)
    {
        tick++;
        if (tick % 1000 == 0)
            rtc_read(&current_time);
    }
    if (irq_num >= 40)
        outb(0xA0, 0x20);
    outb(0x20, 0x20);
}