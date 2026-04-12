#ifndef SBURB_INTERRUPTLIB_H
#define SBURB_INTERRUPTLIB_H

#include <stdint.h>
#include "SBURB_timelib.h"
#include "SBURB_fontlib.h"
#include "SBURB_mouselib.h"
#include "SBURB_portlib.h"
#include "SBURB_globals.h"
#include "SBURB_strlib.h"

#define IDT_ENTRIES 256

// IDT entry structure
struct idt_entry
{
    uint16_t offset_low;
    uint16_t selector;
    uint8_t ist;
    uint8_t type_attr;
    uint16_t offset_mid;
    uint32_t offset_high;
    uint32_t reserved;
} __attribute__((packed));

// IDT pointer structure
struct idt_ptr
{
    uint16_t limit;
    uint64_t base;
} __attribute__((packed));

extern struct idt_entry idt[IDT_ENTRIES];
extern struct idt_ptr idtp;

extern void irq0();
extern void irq1();
extern void irq12();

void idt_init(void);

#endif // SBURB_INTERRUPTLIB_H