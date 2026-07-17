#include <kernel/arch/x86_64/idt.h>
#include <kernel/arch/x86_64/gdt.h>
#include <kernel/types.h>

#define IDT_ENTRIES 256

struct idt_entry {
    u16 offset_low;
    u16 selector;
    u8  ist;
    u8  type_attr;
    u16 offset_mid;
    u32 offset_high;
    u32 zero;
} __attribute__((packed));

struct idt_ptr {
    u16 limit;
    u64 base;
} __attribute__((packed));

static struct idt_entry idt[IDT_ENTRIES];
static struct idt_ptr   idtr;

extern void isr0(void);
extern void isr1(void);
extern void isr2(void);
extern void isr3(void);
extern void isr4(void);
extern void isr5(void);
extern void isr6(void);
extern void isr7(void);
extern void isr8(void);
extern void isr9(void);
extern void isr10(void);
extern void isr11(void);
extern void isr12(void);
extern void isr13(void);
extern void isr14(void);
extern void isr15(void);
extern void isr16(void);
extern void isr17(void);
extern void isr18(void);
extern void isr19(void);
extern void isr20(void);
extern void isr21(void);
extern void isr22(void);
extern void isr23(void);
extern void isr24(void);
extern void isr25(void);
extern void isr26(void);
extern void isr27(void);
extern void isr28(void);
extern void isr29(void);
extern void isr30(void);
extern void isr31(void);
extern void irq0(void);
extern void irq1(void);
extern void isr_default(void);

static void idt_set_gate(u8 vec, void *handler, u16 selector, u8 flags) {
    u64 addr = (u64)handler;
    idt[vec].offset_low  = (u16)(addr & 0xFFFF);
    idt[vec].selector    = selector;
    idt[vec].ist          = 0;
    idt[vec].type_attr    = flags;
    idt[vec].offset_mid   = (u16)((addr >> 16) & 0xFFFF);
    idt[vec].offset_high  = (u32)((addr >> 32) & 0xFFFFFFFF);
    idt[vec].zero          = 0;
}

void idt_init(void) {
    for (int i = 0; i < IDT_ENTRIES; i++) {
        idt_set_gate((u8)i, (void *)isr_default, GDT_KERNEL_CODE, 0x8E);
    }

    idt_set_gate(0, (void *)isr0, GDT_KERNEL_CODE, 0x8E);
    idt_set_gate(1, (void *)isr1, GDT_KERNEL_CODE, 0x8E);
    idt_set_gate(2, (void *)isr2, GDT_KERNEL_CODE, 0x8E);
    idt_set_gate(3, (void *)isr3, GDT_KERNEL_CODE, 0x8E);
    idt_set_gate(4, (void *)isr4, GDT_KERNEL_CODE, 0x8E);
    idt_set_gate(5, (void *)isr5, GDT_KERNEL_CODE, 0x8E);
    idt_set_gate(6, (void *)isr6, GDT_KERNEL_CODE, 0x8E);
    idt_set_gate(7, (void *)isr7, GDT_KERNEL_CODE, 0x8E);
    idt_set_gate(8, (void *)isr8, GDT_KERNEL_CODE, 0x8E);
    idt_set_gate(9, (void *)isr9, GDT_KERNEL_CODE, 0x8E);
    idt_set_gate(10, (void *)isr10, GDT_KERNEL_CODE, 0x8E);
    idt_set_gate(11, (void *)isr11, GDT_KERNEL_CODE, 0x8E);
    idt_set_gate(12, (void *)isr12, GDT_KERNEL_CODE, 0x8E);
    idt_set_gate(13, (void *)isr13, GDT_KERNEL_CODE, 0x8E);
    idt_set_gate(14, (void *)isr14, GDT_KERNEL_CODE, 0x8E);
    idt_set_gate(15, (void *)isr15, GDT_KERNEL_CODE, 0x8E);
    idt_set_gate(16, (void *)isr16, GDT_KERNEL_CODE, 0x8E);
    idt_set_gate(17, (void *)isr17, GDT_KERNEL_CODE, 0x8E);
    idt_set_gate(18, (void *)isr18, GDT_KERNEL_CODE, 0x8E);
    idt_set_gate(19, (void *)isr19, GDT_KERNEL_CODE, 0x8E);
    idt_set_gate(20, (void *)isr20, GDT_KERNEL_CODE, 0x8E);
    idt_set_gate(21, (void *)isr21, GDT_KERNEL_CODE, 0x8E);
    idt_set_gate(22, (void *)isr22, GDT_KERNEL_CODE, 0x8E);
    idt_set_gate(23, (void *)isr23, GDT_KERNEL_CODE, 0x8E);
    idt_set_gate(24, (void *)isr24, GDT_KERNEL_CODE, 0x8E);
    idt_set_gate(25, (void *)isr25, GDT_KERNEL_CODE, 0x8E);
    idt_set_gate(26, (void *)isr26, GDT_KERNEL_CODE, 0x8E);
    idt_set_gate(27, (void *)isr27, GDT_KERNEL_CODE, 0x8E);
    idt_set_gate(28, (void *)isr28, GDT_KERNEL_CODE, 0x8E);
    idt_set_gate(29, (void *)isr29, GDT_KERNEL_CODE, 0x8E);
    idt_set_gate(30, (void *)isr30, GDT_KERNEL_CODE, 0x8E);
    idt_set_gate(31, (void *)isr31, GDT_KERNEL_CODE, 0x8E);

    idt_set_gate(32, (void *)irq0, GDT_KERNEL_CODE, 0x8E);
    idt_set_gate(33, (void *)irq1, GDT_KERNEL_CODE, 0x8E);

    idtr.limit = sizeof(idt) - 1;
    idtr.base  = (u64)&idt;
    __asm__ volatile ("lidt %0" : : "m"(idtr));
}
