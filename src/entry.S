.section .text
.global _start

_start:
    # Disable interrupts
    cli

    # Set up stack using absolute address
    mov $stack_top, %rsp

    # Align stack to 16-byte boundary (required by System V ABI)
    and $-16, %rsp
    sub $8, %rsp  # Account for return address

    # Call C entry point
    call kmain

    # Should not return, but halt if it does
    cli
    hlt
    jmp _start

.section .bss
.align 4096
stack_bottom:
    .space 65536  # 64 KB stack
stack_top:

