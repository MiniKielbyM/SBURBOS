.section .text
.global _start

_start:
    # Disable interrupts
    cli
    
    # Set up stack
    lea stack_top(%rip), %rsp
    
    # Align stack to 16-byte boundary (required by System V ABI)
    and $-16, %rsp
    
    # Call _start_c which will set up everything else
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
