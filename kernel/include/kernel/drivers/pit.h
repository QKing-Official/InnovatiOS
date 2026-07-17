#ifndef KERNEL_DRIVERS_PIT_H
#define KERNEL_DRIVERS_PIT_H

#include <kernel/types.h>

void pit_init(u32 hz);
void pit_handle_irq(void);
u64 pit_get_ticks(void);
void pit_sleep_ms(u32 ms);

#endif
