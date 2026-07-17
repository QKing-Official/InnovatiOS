#ifndef KERNEL_MM_VMM_H
#define KERNEL_MM_VMM_H

#include <kernel/types.h>

#define VMM_PAGE_SIZE 4096

#define VMM_FLAG_PRESENT  (1ULL << 0)
#define VMM_FLAG_WRITABLE (1ULL << 1)
#define VMM_FLAG_USER     (1ULL << 2)
#define VMM_FLAG_NX       (1ULL << 63)

typedef u64 vmm_space_t;

void vmm_init(u64 hhdm_offset);
int vmm_map(vmm_space_t space, u64 vaddr, u64 paddr, u64 flags);
void vmm_unmap(vmm_space_t space, u64 vaddr);
u64 vmm_translate(vmm_space_t space, u64 vaddr);
vmm_space_t vmm_kernel_space(void);

#endif
