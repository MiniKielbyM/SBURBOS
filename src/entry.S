.section .text

.global _start
_start:
    # Set up stack pointer with RIP-relative addressing
    lea stack_top(%rip), %rsp
    
    # Align stack to 16 bytes as required by x86-64 ABI
    and $-16, %rsp
    
    # Call the kernel main function
    call kmain
    
    # Halt if kmain returns
    cli
    hlt
    jmp _start

.section .bss
.align 4096
stack_bottom:
    .space 32768  # 32 KB stack
stack_top:
