#ifndef KERNEL_MM_PMM_H
#define KERNEL_MM_PMM_H

#include <kernel/types.h>

#define PMM_PAGE_SIZE 4096

void pmm_init(void *limine_memmap_response, u64 hhdm_offset);
u64 pmm_alloc_page(void);
u64 pmm_alloc_pages(u64 count);
void pmm_free_page(u64 phys_addr);
void pmm_free_pages(u64 phys_addr, u64 count);
u64 pmm_total_pages(void);
u64 pmm_free_pages_count(void);

#endif
