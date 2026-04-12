.section .text

.global irq0
.global irq1
.global irq12

irq0:
    pushq $0
    pushq $32
    jmp irq_common_stub

irq1:
    pushq $0
    pushq $33
    jmp irq_common_stub

irq12:
    pushq $0
    pushq $44
    jmp irq_common_stub

.global irq_common_stub
irq_common_stub:
    push %rax
    push %rbx
    push %rcx
    push %rdx
    push %rsi
    push %rdi
    push %rbp

    mov %rsp, %rdi
    call irq_handler

    pop %rbp
    pop %rdi
    pop %rsi
    pop %rdx
    pop %rcx
    pop %rbx
    pop %rax

    add $16, %rsp
    iretq