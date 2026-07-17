#include <kernel/mm/vmm.h>
#include <kernel/mm/pmm.h>
#include <kernel/lib/string.h>
#include <kernel/drivers/serial.h>

#define ENTRIES_PER_TABLE 512
#define ADDR_MASK ~((u64)0xFFF)

#define PML4_INDEX(vaddr) (((vaddr) >> 39) & 0x1FF)
#define PDPT_INDEX(vaddr) (((vaddr) >> 30) & 0x1FF)
#define PD_INDEX(vaddr)   (((vaddr) >> 21) & 0x1FF)
#define PT_INDEX(vaddr)   (((vaddr) >> 12) & 0x1FF)

static u64 g_hhdm_offset;
static vmm_space_t g_kernel_space;

static inline u64 *phys_to_virt(u64 phys) {
    return (u64 *)(phys + g_hhdm_offset);
}

static u64 *get_or_create_table(u64 *table, u64 index) {
    if (!(table[index] & VMM_FLAG_PRESENT)) {
        u64 new_phys = pmm_alloc_page();
        if (new_phys == 0) return NULL;

        u64 *new_virt = phys_to_virt(new_phys);
        k_memset(new_virt, 0, VMM_PAGE_SIZE);

        table[index] = new_phys | VMM_FLAG_PRESENT | VMM_FLAG_WRITABLE | VMM_FLAG_USER;
    }
    return phys_to_virt(table[index] & ADDR_MASK);
}

static u64 *get_table_or_null(u64 *table, u64 index) {
    if (!(table[index] & VMM_FLAG_PRESENT)) return NULL;
    return phys_to_virt(table[index] & ADDR_MASK);
}

void vmm_init(u64 hhdm_offset) {
    g_hhdm_offset = hhdm_offset;

    u64 cr3;
    __asm__ volatile ("mov %%cr3, %0" : "=r"(cr3));
    g_kernel_space = cr3 & ADDR_MASK;

    serial_write("vmm: adopted existing CR3 as the kernel address space\n");
}

vmm_space_t vmm_kernel_space(void) {
    return g_kernel_space;
}

int vmm_map(vmm_space_t space, u64 vaddr, u64 paddr, u64 flags) {
    u64 *pml4 = phys_to_virt(space);

    u64 *pdpt = get_or_create_table(pml4, PML4_INDEX(vaddr));
    if (!pdpt) return -1;
    u64 *pd = get_or_create_table(pdpt, PDPT_INDEX(vaddr));
    if (!pd) return -1;
    u64 *pt = get_or_create_table(pd, PD_INDEX(vaddr));
    if (!pt) return -1;

    pt[PT_INDEX(vaddr)] = (paddr & ADDR_MASK) | flags;

    __asm__ volatile ("invlpg (%0)" : : "r"(vaddr) : "memory");
    return 0;
}

void vmm_unmap(vmm_space_t space, u64 vaddr) {
    u64 *pml4 = phys_to_virt(space);

    u64 *pdpt = get_table_or_null(pml4, PML4_INDEX(vaddr));
    if (!pdpt) return;
    u64 *pd = get_table_or_null(pdpt, PDPT_INDEX(vaddr));
    if (!pd) return;
    u64 *pt = get_table_or_null(pd, PD_INDEX(vaddr));
    if (!pt) return;

    pt[PT_INDEX(vaddr)] = 0;
    __asm__ volatile ("invlpg (%0)" : : "r"(vaddr) : "memory");
}

u64 vmm_translate(vmm_space_t space, u64 vaddr) {
    u64 *pml4 = phys_to_virt(space);

    u64 *pdpt = get_table_or_null(pml4, PML4_INDEX(vaddr));
    if (!pdpt) return 0;
    u64 *pd = get_table_or_null(pdpt, PDPT_INDEX(vaddr));
    if (!pd) return 0;
    u64 *pt = get_table_or_null(pd, PD_INDEX(vaddr));
    if (!pt) return 0;

    u64 pte = pt[PT_INDEX(vaddr)];
    if (!(pte & VMM_FLAG_PRESENT)) return 0;

    return (pte & ADDR_MASK) | (vaddr & 0xFFF);
}
