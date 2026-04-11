#include ".h/SBURB_interruptlib.h"

struct idt_entry idt[IDT_ENTRIES];
struct idt_ptr idtp;

// helper to set an IDT gate
void idt_set_gate(int n, void (*handler)())
{
    uint64_t addr = (uint64_t)handler;

    idt[n].offset_low = addr & 0xFFFF;
    idt[n].selector = 0x28; // Limine kernel code segment
    idt[n].ist = 0;
    idt[n].type_attr = 0x8E; // interrupt gate, present, ring 0
    idt[n].offset_mid = (addr >> 16) & 0xFFFF;
    idt[n].offset_high = (addr >> 32) & 0xFFFFFFFF;
    idt[n].reserved = 0;
}

void idt_init(void)
{
    // zero entire IDT
    for (int i = 0; i < IDT_ENTRIES; i++)
    {
        idt[i] = (struct idt_entry){0};
    }

    // IRQs (after PIC remap)
    idt_set_gate(32, irq0); // timer
    idt_set_gate(33, irq1); // keyboard

    // load IDT
    idtp.limit = sizeof(idt) - 1;
    idtp.base = (uint64_t)&idt;

    __asm__ volatile("lidt %0" : : "m"(idtp));
}