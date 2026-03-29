.section .text

.global _start
_start:
    # Set up stack pointer
    mov $stack_top, %rsp
    
    # Align stack to 16 bytes as required by x86-64 ABI
    and $-16, %rsp
    
    # Call the kernel main function
    call kmain
    
    # Halt if kmain returns
    cli
    hlt
    jmp .

.section .bss
.align 16
stack_bottom:
    .space 16384  # 16 KB stack
stack_top:
