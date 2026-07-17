#include <kernel/arch/x86_64/pic.h>
#include <kernel/arch/x86_64/io.h>

#define PIC1_CMD  0x20
#define PIC1_DATA 0x21
#define PIC2_CMD  0xA0
#define PIC2_DATA 0xA1

#define ICW1_INIT 0x10
#define ICW1_ICW4 0x01
#define ICW4_8086 0x01

void pic_remap(void) {
    outb(PIC1_CMD, ICW1_INIT | ICW1_ICW4); io_wait();
    outb(PIC2_CMD, ICW1_INIT | ICW1_ICW4); io_wait();

    outb(PIC1_DATA, 32); io_wait();
    outb(PIC2_DATA, 40); io_wait();

    outb(PIC1_DATA, 4); io_wait();
    outb(PIC2_DATA, 2); io_wait();

    outb(PIC1_DATA, ICW4_8086); io_wait();
    outb(PIC2_DATA, ICW4_8086); io_wait();

    outb(PIC1_DATA, 0xFF);
    outb(PIC2_DATA, 0xFF);

    u8 master_mask = inb(PIC1_DATA);
    master_mask &= (u8)~(1 << 0);
    outb(PIC1_DATA, master_mask);
}

void pic_send_eoi(u8 irq) {
    if (irq >= 8) {
        outb(PIC2_CMD, 0x20);
    }
    outb(PIC1_CMD, 0x20);
}

void pic_unmask_irq(u8 irq) {
    u16 port = (irq < 8) ? PIC1_DATA : PIC2_DATA;
    u8 line = (irq < 8) ? irq : (u8)(irq - 8);
    u8 mask = inb(port);
    mask &= (u8)~(1u << line);
    outb(port, mask);
}

void pic_mask_irq(u8 irq) {
    u16 port = (irq < 8) ? PIC1_DATA : PIC2_DATA;
    u8 line = (irq < 8) ? irq : (u8)(irq - 8);
    u8 mask = inb(port);
    mask |= (u8)(1u << line);
    outb(port, mask);
}
