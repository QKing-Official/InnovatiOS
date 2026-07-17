#ifndef KERNEL_PROC_SCHEDULER_H
#define KERNEL_PROC_SCHEDULER_H

#include <kernel/types.h>
#include <kernel/arch/x86_64/interrupts.h>

void scheduler_init(u64 hhdm_offset);
void scheduler_on_timer(interrupt_frame_t *frame);
void scheduler_start_demo(void) __attribute__((noreturn));

#endif
