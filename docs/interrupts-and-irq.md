# Interrupts and IRQs on x86-64

This document covers how hardware interrupts work on the **x86-64** (aka AMD64 / Intel 64) architecture, how SBURBOS currently handles keyboard input, and how to transition from polling to interrupt-driven I/O.

> **Architecture-specific**: Everything in this document is specific to x86-64. ARM, RISC-V, and other architectures have completely different interrupt controllers, descriptor tables, and privilege models. The concepts (hardware signals interrupting the CPU) are universal, but the implementation details are not.

---

## Table of Contents

1. [Background: What is an Interrupt?](#background-what-is-an-interrupt)
2. [Polling vs. Interrupts](#polling-vs-interrupts)
3. [x86-64 Interrupt Architecture](#x86-64-interrupt-architecture)
4. [The PIC (Programmable Interrupt Controller)](#the-pic-programmable-interrupt-controller)
5. [The IDT (Interrupt Descriptor Table)](#the-idt-interrupt-descriptor-table)
6. [Key Assembly Instructions: CLI, STI, HLT, IRETQ](#key-assembly-instructions-cli-sti-hlt-iretq)
7. [Why Inline Assembly is Needed](#why-inline-assembly-is-needed)
8. [Current SBURBOS Implementation](#current-sburbos-implementation)
9. [Implementation Plan: IRQ-Based Keyboard](#implementation-plan-irq-based-keyboard)
10. [Future: Timers via IRQ0](#future-timers-via-irq0)
11. [References](#references)

---

## Background: What is an Interrupt?

An **interrupt** is a signal that tells the CPU to stop what it's doing and run a specific handler function. There are three kinds:

| Type | Source | Example |
|------|--------|---------|
| **Hardware interrupt (IRQ)** | External device via a wire to the CPU | Keyboard key press, timer tick, disk I/O complete |
| **Software interrupt** | `int` instruction in code | `int 0x80` (legacy Linux syscall) |
| **CPU exception** | CPU itself, on error | Division by zero, page fault, general protection fault |

Hardware interrupts are **asynchronous** — they can fire at any time, between any two instructions. The CPU finishes the current instruction, saves its state, looks up the handler in the **IDT** (Interrupt Descriptor Table), jumps to it, and when the handler returns via `iretq`, resumes where it left off.

---

## Polling vs. Interrupts

### Polling (what SBURBOS does now)

```
while (true) {
    if (device_has_data()) {   // check over and over
        data = read_device();
        process(data);
    }
}
```

- CPU runs at 100% checking the device in a tight loop
- Nothing else can execute — the CPU is trapped in the loop
- Simple to implement, but wasteful and unscalable
- If you need to handle two devices, you have to interleave checks for both in the same loop

### Interrupts (the goal)

```
// Set up handler once
register_handler(KEYBOARD_IRQ, keyboard_handler);
enable_interrupts();

while (true) {
    halt();  // CPU sleeps, wakes ONLY when an interrupt fires
}
```

- CPU is idle (near-zero power) until something actually happens
- Handler runs automatically when the device signals
- Multiple devices can each have their own handler
- Foundation for multitasking — a timer interrupt can switch between processes

---

## x86-64 Interrupt Architecture

The flow when a key is pressed:

```
Keyboard hardware
    |
    | electrical signal on IRQ line 1
    v
PIC (Programmable Interrupt Controller)
    |
    | translates IRQ1 -> interrupt vector 33 (after remapping)
    | sends signal to CPU
    v
CPU
    | 1. finishes current instruction
    | 2. pushes SS, RSP, RFLAGS, CS, RIP onto the stack (automatic)
    | 3. looks up entry 33 in the IDT
    | 4. jumps to the handler address stored there
    v
irq1 (our assembly stub in irq_stubs.S)
    | 5. saves additional registers (rax, rbx, etc.)
    | 6. calls irq_handler() in C
    v
irq_handler() in C
    | 7. reads scancode from port 0x60
    | 8. processes the key
    | 9. sends EOI to PIC (tells it we're done)
    | 10. returns
    v
irq1 stub
    | 11. restores registers
    | 12. executes iretq
    v
CPU resumes whatever it was doing before the interrupt
```

### Interrupt Vectors

x86-64 supports 256 interrupt vectors (0-255):

| Range | Purpose |
|-------|---------|
| 0-31 | CPU exceptions (reserved by Intel). Division error, page fault, GPF, etc. |
| 32-47 | Hardware IRQs (after PIC remapping). 32 = timer, 33 = keyboard, etc. |
| 48-255 | Available for software use (syscalls, IPI, etc.) |

---

## The PIC (Programmable Interrupt Controller)

The **8259 PIC** is the chip (or emulated equivalent) that sits between hardware devices and the CPU. There are actually two PICs chained together:

```
IRQ 0  (Timer)        ----\
IRQ 1  (Keyboard)     -----\
IRQ 2  (Cascade)      ------+-- Master PIC (port 0x20/0x21) --> CPU
IRQ 3  (COM2)         -----/
IRQ 4  (COM1)         ----/
IRQ 5  (LPT2)         ---/
IRQ 6  (Floppy)       --/
IRQ 7  (LPT1)         -/

IRQ 8  (RTC)          ----\
IRQ 9  (ACPI)         -----\
IRQ 10 (Open)         ------+-- Slave PIC (port 0xA0/0xA1) --> Master PIC via IRQ 2
IRQ 11 (Open)         -----/
IRQ 12 (PS/2 Mouse)   ----/
IRQ 13 (FPU)          ---/
IRQ 14 (Primary ATA)  --/
IRQ 15 (Secondary ATA)-/
```

### Why remap the PIC?

By default, the master PIC maps IRQs 0-7 to interrupt vectors 0-7. But vectors 0-31 are reserved for CPU exceptions (e.g., vector 8 = Double Fault). So IRQ 0 (timer) would collide with vector 0 (Division Error). **Remapping** moves the IRQs to vectors 32-47 to avoid this collision.

### PIC Remapping Sequence

This is a fixed protocol defined by the 8259 chip — four "Initialization Command Words" (ICW1-ICW4) sent to specific I/O ports:

```c
void pic_remap(void) {
    // ICW1: Start initialization sequence (cascade mode, ICW4 needed)
    outb(0x20, 0x11);   // master command port
    outb(0xA0, 0x11);   // slave command port

    // ICW2: Set base interrupt vector
    outb(0x21, 0x20);   // master: IRQs 0-7  -> vectors 32-39
    outb(0xA1, 0x28);   // slave:  IRQs 8-15 -> vectors 40-47

    // ICW3: Configure cascade (how master/slave are wired)
    outb(0x21, 0x04);   // master: slave is on IRQ 2
    outb(0xA1, 0x02);   // slave:  connected to master's IRQ 2

    // ICW4: Set 8086 mode
    outb(0x21, 0x01);
    outb(0xA1, 0x01);

    // Set interrupt masks (1 = masked/disabled, 0 = enabled)
    outb(0x21, 0xFC);   // master: enable IRQ 0 (timer) and IRQ 1 (keyboard)
    outb(0xA1, 0xFF);   // slave:  mask everything
}
```

The mask byte `0xFC` = `11111100` in binary — bits 0 and 1 are clear, enabling IRQ 0 (timer) and IRQ 1 (keyboard).

### End of Interrupt (EOI)

After handling an IRQ, you **must** send an EOI signal to the PIC, or it won't send you the next interrupt:

```c
outb(0x20, 0x20);  // EOI to master PIC
```

For IRQs 8-15 (slave PIC), you need to send EOI to both:

```c
outb(0xA0, 0x20);  // EOI to slave
outb(0x20, 0x20);  // EOI to master
```

---

## The IDT (Interrupt Descriptor Table)

The IDT is an array of 256 entries that tells the CPU where to jump for each interrupt vector. On x86-64, each entry is 16 bytes:

```c
struct idt_entry {
    uint16_t offset_low;   // bits 0-15 of handler address
    uint16_t selector;     // code segment selector from GDT
    uint8_t  ist;          // Interrupt Stack Table offset (0 = don't use)
    uint8_t  type_attr;    // gate type, DPL, and present bit
    uint16_t offset_mid;   // bits 16-31 of handler address
    uint32_t offset_high;  // bits 32-63 of handler address
    uint32_t reserved;     // must be zero
} __attribute__((packed));
```

The handler address is split across three fields because this format evolved from 16-bit and 32-bit predecessors.

### type_attr byte

```
  7   6 5   4   3 2 1 0
+---+-----+---+---------+
| P | DPL | 0 |  Type   |
+---+-----+---+---------+

P    = 1 (present / valid entry)
DPL  = 00 (ring 0 only — kernel)
Type = 1110 (64-bit interrupt gate)

Combined: 0b10001110 = 0x8E
```

### Loading the IDT

The CPU has a special register (IDTR) that points to the IDT. You load it with the `lidt` instruction:

```c
struct idt_pointer {
    uint16_t limit;   // size of IDT in bytes - 1
    uint64_t base;    // address of idt[0]
} __attribute__((packed));

struct idt_pointer idtp = {
    .limit = sizeof(idt) - 1,
    .base  = (uint64_t)&idt
};

__asm__ volatile("lidt %0" : : "m"(idtp));
```

### Code Segment Selector

The `selector` field in each IDT entry must point to a valid 64-bit code segment in the GDT. Limine (the bootloader SBURBOS uses) sets up a GDT with the kernel code segment at offset `0x28`. This value may vary if you set up your own GDT later.

---

## Key Assembly Instructions: CLI, STI, HLT, IRETQ

These are **x86 CPU instructions** that have no C equivalent. They directly control hardware behavior that C has no concept of.

### `cli` — Clear Interrupt Flag

```nasm
cli
```

**Disables hardware interrupts.** Sets the IF (Interrupt Flag) in the RFLAGS register to 0. While IF=0, the CPU ignores all maskable hardware interrupts (IRQs). CPU exceptions (like page faults) still fire.

Use it when modifying shared data structures that interrupt handlers also touch, to prevent a handler from running mid-update.

### `sti` — Set Interrupt Flag

```nasm
sti
```

**Enables hardware interrupts.** Sets IF=1. The CPU will begin accepting IRQs again. If an interrupt was pending while disabled, it fires immediately after `sti`.

### `hlt` — Halt

```nasm
hlt
```

**Puts the CPU into a low-power sleep state** until the next interrupt arrives. When an interrupt fires, the CPU wakes up, handles it, and execution continues after the `hlt`. This is how an idle kernel avoids burning CPU cycles. Without `hlt`, you'd need a busy-wait loop.

### `iretq` — Interrupt Return (64-bit)

```nasm
iretq
```

**Returns from an interrupt handler.** Pops RIP, CS, RFLAGS, RSP, and SS from the stack (the values the CPU pushed when the interrupt fired) and resumes execution at the interrupted code. This is in `irq_stubs.S` at line 38.

### Usage in an idle loop

```c
__asm__ volatile("sti");     // enable interrupts
while (1) {
    __asm__ volatile("hlt"); // sleep until interrupt
    // after handling the interrupt, we loop back and hlt again
}
```

---

## Why Inline Assembly is Needed

C is a portable language — it doesn't have keywords for CPU-specific operations like enabling/disabling interrupts or halting the processor. These are **privileged instructions** that directly manipulate CPU registers and state.

There is no way to express these in pure C:

| Instruction | What it does | Why C can't do it |
|-------------|-------------|-------------------|
| `cli` / `sti` | Toggle the interrupt flag in RFLAGS | C has no concept of CPU flags |
| `hlt` | Halt the CPU until interrupt | C has no "sleep until hardware event" primitive |
| `iretq` | Special return from interrupt (pops 5 values) | Not a normal function return — different stack behavior |
| `lidt` | Load IDT register | Dedicated CPU register, not memory |
| `inb` / `outb` | Read/write I/O ports | x86-specific I/O address space, not memory |

GCC's `__asm__ volatile(...)` syntax lets you embed individual assembly instructions inside C code. The `volatile` keyword tells the compiler "don't optimize this away or reorder it — it has side effects you can't see."

For more complex sequences (like the IRQ stubs that need precise stack manipulation), we write full assembly files like `irq_stubs.S`.

---

## Current SBURBOS Implementation

### What exists

| Component | File | Status |
|-----------|------|--------|
| Keyboard polling | `src/main.c:664-672, 819-851` | Working, but busy-waits |
| Scancode map | `src/main.c:421-426` | Working |
| I/O port functions (`inb`/`outb`) | `src/main.c:508-518` | Working |
| IRQ assembly stubs (`irq0`, `irq1`) | `src/irq_stubs.S` | Written, but not connected |
| `irq_handler()` C function | — | **Not implemented** |
| IDT setup | — | **Not implemented** |
| PIC initialization | — | **Not implemented** |
| `sti` (enable interrupts) | — | **Never called** — `cli` at `main.c:819` disables forever |

### The polling loop (current)

```c
// src/main.c:819-851
__asm__ volatile("cli");  // disable interrupts (and never re-enable them)
while (1) {
    if (keyboard_has_data()) {        // spin checking port 0x64
        uint8_t sc = keyboard_read(); // read from port 0x60
        if (!(sc & 0x80)) {           // ignore key release
            char c = scancode_map[sc];
            // ... handle character
        }
    }
    // if no key pressed, immediately loop back and check again
    // CPU is at 100% utilization doing nothing useful
}
```

---

## Implementation Plan: IRQ-Based Keyboard

### Step 1: PIC remapping

Remap IRQs to vectors 32+ and unmask IRQ 0 (timer) and IRQ 1 (keyboard).

### Step 2: IDT setup

Create a 256-entry IDT, populate entries 32 and 33 with pointers to `irq0` and `irq1` from `irq_stubs.S`, and load it with `lidt`.

**Note on the selector value**: Limine's default GDT places the 64-bit kernel code segment at offset `0x28`. This is what goes in the `selector` field of each IDT entry.

### Step 3: Implement `irq_handler()` in C

```c
void irq_handler(uint64_t *regs) {
    // The IRQ number was pushed onto the stack by the assembly stub
    // It's at a known offset in the saved register array
    uint64_t irq_num = regs[7];  // depends on push order in irq_stubs.S

    if (irq_num == 33) {  // IRQ 1 = keyboard (remapped to vector 33)
        uint8_t sc = inb(0x60);
        if (!(sc & 0x80)) {
            char c = scancode_map[sc];
            if (c == '\b') {
                delete_last_char();
            } else if (c == '\t') {
                print("    ");
            } else if (c != 0) {
                print((char[]){c, '\0'});
            }
        }
    }

    // Send End of Interrupt to PIC
    if (irq_num >= 40) {
        outb(0xA0, 0x20);  // EOI to slave PIC
    }
    outb(0x20, 0x20);      // EOI to master PIC
}
```

### Step 4: Replace polling loop

```c
// old:
__asm__ volatile("cli");
while (1) {
    if (keyboard_has_data()) { ... }
}

// new:
pic_remap();
idt_init();  // set up IDT and load with lidt
__asm__ volatile("sti");
while (1) {
    __asm__ volatile("hlt");
}
```

---

## Future: Timers via IRQ0

Once the IRQ infrastructure is in place, adding a timer is straightforward. The **PIT (Programmable Interval Timer)** generates IRQ 0 at a configurable frequency.

### PIT setup

```c
void pit_init(uint32_t frequency_hz) {
    uint16_t divisor = 1193180 / frequency_hz;  // PIT base clock is ~1.193 MHz
    outb(0x43, 0x36);                // channel 0, lobyte/hibyte, square wave
    outb(0x40, divisor & 0xFF);      // low byte
    outb(0x40, (divisor >> 8) & 0xFF); // high byte
}
```

### Timer handler

Add a branch in `irq_handler()`:

```c
static volatile uint64_t ticks = 0;

if (irq_num == 32) {  // IRQ 0 = timer
    ticks++;
    // future: task switching, sleep() implementation, animations, etc.
}
```

This gives you a heartbeat for the entire OS — scheduling, delays, timeouts, animations, all driven by the timer tick.

---

## References

- [OSDev Wiki: Interrupts](https://wiki.osdev.org/Interrupts) — comprehensive overview
- [OSDev Wiki: IDT](https://wiki.osdev.org/Interrupt_Descriptor_Table) — IDT structure and setup
- [OSDev Wiki: 8259 PIC](https://wiki.osdev.org/8259_PIC) — PIC remapping and programming
- [OSDev Wiki: IRQ](https://wiki.osdev.org/IRQ) — IRQ routing and handling
- [OSDev Wiki: Inline Assembly](https://wiki.osdev.org/Inline_Assembly) — GCC inline asm syntax
- [OSDev Wiki: PIT](https://wiki.osdev.org/Programmable_Interval_Timer) — timer configuration
- [Intel SDM Vol. 3, Chapter 6](https://www.intel.com/content/www/us/en/developer/articles/technical/intel-sdm.html) — Interrupt and Exception Handling (authoritative reference)
- [Limine Boot Protocol](https://github.com/limine-bootloader/limine/blob/trunk/PROTOCOL.md) — GDT and initial machine state after boot
