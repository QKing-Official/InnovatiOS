#ifndef KERNEL_ARCH_X86_64_INTERRUPTS_H
#define KERNEL_ARCH_X86_64_INTERRUPTS_H

#include <kernel/types.h>

typedef struct interrupt_frame {
    u64 r15;
    u64 r14;
    u64 r13;
    u64 r12;
    u64 r11;
    u64 r10;
    u64 r9;
    u64 r8;
    u64 rdi;
    u64 rsi;
    u64 rbp;
    u64 rdx;
    u64 rcx;
    u64 rbx;
    u64 rax;
    u64 vector;
    u64 error_code;
    u64 rip;
    u64 cs;
    u64 rflags;
    u64 rsp;
    u64 ss;
} interrupt_frame_t;

void arch_interrupt_dispatch(interrupt_frame_t *frame);
void exception_demo_showcase(void);

#endif
