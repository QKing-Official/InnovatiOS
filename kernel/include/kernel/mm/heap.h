#ifndef KERNEL_MM_HEAP_H
#define KERNEL_MM_HEAP_H

#include <kernel/types.h>

void heap_init(u64 hhdm_offset, u64 initial_pages);
void *kmalloc(size_t size);
void  kfree(void *ptr);

#endif
