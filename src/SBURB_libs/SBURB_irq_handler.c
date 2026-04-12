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
    if (irq_num == 44)
    {
        println("Mouse IRQ received");
        // mouse IRQ
        uint8_t status = inb(0x64);
        if (status & 0x20) // data from mouse
        {
            uint8_t data = inb(0x60);
            static struct mouse_event event;
            static int byte_num = 0;

            if (byte_num == 0)
            {
                event.button = data & 0x07;
                event.x = (data & 0x10) ? (data - 256) : data;
                byte_num++;
            }
            else if (byte_num == 1)
            {
                event.y = (data & 0x10) ? (data - 256) : data;
                byte_num++;
            }
            else if (byte_num == 2)
            {
                mouse_handler(event);
                byte_num = 0;
            }
        }
    }
    if (irq_num >= 40)
        outb(0xA0, 0x20);
    outb(0x20, 0x20);
}