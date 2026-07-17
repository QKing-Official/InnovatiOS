#include <kernel/arch/x86_64/gdt.h>
#include <kernel/lib/string.h>
#include <kernel/types.h>

#define KERNEL_STACK_SIZE (16 * 1024)

struct tss_entry {
    u32 reserved0;
    u64 rsp0;
    u64 rsp1;
    u64 rsp2;
    u64 reserved1;
    u64 ist1;
    u64 ist2;
    u64 ist3;
    u64 ist4;
    u64 ist5;
    u64 ist6;
    u64 ist7;
    u64 reserved2;
    u16 reserved3;
    u16 iopb_offset;
} __attribute__((packed));

struct gdt_ptr {
    u16 limit;
    u64 base;
} __attribute__((packed));

static u64 gdt[7];
static struct gdt_ptr gdtr;
static struct tss_entry tss;
static u8 kernel_stack[KERNEL_STACK_SIZE] __attribute__((aligned(16)));

static void set_tss_descriptor(u64 base, u32 limit) {
    u64 lo = (limit & 0xFFFFULL)
           | ((base & 0xFFFFFFULL) << 16)
           | (0x89ULL << 40)
           | (((u64)(limit >> 16) & 0xFULL) << 48)
           | (((base >> 24) & 0xFFULL) << 56);
    u64 hi = (base >> 32) & 0xFFFFFFFFULL;

    gdt[5] = lo;
    gdt[6] = hi;
}

void gdt_init(void) {
    k_memset(&tss, 0, sizeof(tss));
    tss.rsp0 = (u64)(kernel_stack + KERNEL_STACK_SIZE);
    tss.iopb_offset = sizeof(tss);

    gdt[0] = 0x0000000000000000ULL;
    gdt[1] = 0x00AF9A000000FFFFULL;
    gdt[2] = 0x00CF92000000FFFFULL;
    gdt[3] = 0x00AFFA000000FFFFULL;
    gdt[4] = 0x00CFF2000000FFFFULL;

    set_tss_descriptor((u64)&tss, sizeof(tss) - 1);

    gdtr.limit = sizeof(gdt) - 1;
    gdtr.base  = (u64)&gdt;

    __asm__ volatile ("lgdt %0" : : "m"(gdtr));

    __asm__ volatile (
        "push $0x08\n"
        "lea 1f(%%rip), %%rax\n"
        "push %%rax\n"
        "lretq\n"
        "1:\n"
        "mov $0x10, %%ax\n"
        "mov %%ax, %%ds\n"
        "mov %%ax, %%es\n"
        "mov %%ax, %%fs\n"
        "mov %%ax, %%gs\n"
        "mov %%ax, %%ss\n"
        : : : "rax", "memory"
    );

    __asm__ volatile ("ltr %0" : : "r"((u16)GDT_TSS));
}
