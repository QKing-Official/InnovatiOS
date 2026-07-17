#ifndef KERNEL_ARCH_X86_64_PIC_H
#define KERNEL_ARCH_X86_64_PIC_H

#include <kernel/types.h>

void pic_remap(void);
void pic_send_eoi(u8 irq);
void pic_unmask_irq(u8 irq);
void pic_mask_irq(u8 irq);

#endif
