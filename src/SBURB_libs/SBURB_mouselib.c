#include ".h/SBURB_mouselib.h"

void mouse_init()
{
    outb(0x64, 0xA8); // enable aux device
    outb(0x64, 0x20); // request current state
    uint8_t state = inb(0x60);
    state |= 2; // enable IRQ12
    outb(0x64, 0x60); // set state
    outb(0x60, state);
    outb(0x60, 0xF4); // reset mouse
}

void mouse_handler(struct mouse_event event)
{
    println("Mouse event received");
    // Process mouse event
    switch (event.type)
    {
    case MOUSE_MOVE:
        char buf[32];
        mouse_x += event.x;
        mouse_y -= event.y; // y is inverted
        println(concat(buf, "Mouse moved to (", concat(buf, itoa(mouse_x, buf, 10), concat(buf, ", ", concat(buf, itoa(mouse_y, buf, 10), ")")))));
        break;
    case MOUSE_CLICK:
        // Handle mouse click
        break;
    }
}