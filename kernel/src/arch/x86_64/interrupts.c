#include <kernel/arch/x86_64/interrupts.h>
#include <kernel/arch/x86_64/pic.h>
#include <kernel/drivers/keyboard.h>
#include <kernel/drivers/pit.h>
#include <kernel/drivers/serial.h>
#include <kernel/proc/scheduler.h>
#include <kernel/lib/string.h>
#include <kernel/arch/x86_64/gdt.h>

static void serial_write_hex64(u64 value) {
    static const char digits[] = "0123456789abcdef";
    char buf[18];
    buf[0] = '0';
    buf[1] = 'x';
    for (int i = 0; i < 16; i++) {
        buf[2 + i] = digits[(value >> (60 - (i * 4))) & 0xFULL];
    }
    serial_write_len(buf, sizeof(buf));
}

static void serial_write_line_hex(const char *label, u64 value) {
    serial_write(label);
    serial_write_hex64(value);
    serial_write("\n");
}

static const char *exception_name(u64 vector) {
    static const char *const names[] = {
        "#DE divide error",
        "#DB debug",
        "NMI",
        "#BP breakpoint",
        "#OF overflow",
        "#BR bound range",
        "#UD invalid opcode",
        "#NM device not available",
        "#DF double fault",
        "coprocessor segment overrun",
        "#TS invalid TSS",
        "#NP segment not present",
        "#SS stack segment fault",
        "#GP general protection",
        "#PF page fault",
        "reserved",
        "x87 floating-point",
        "#AC alignment check",
        "#MC machine check",
        "#XM SIMD floating-point",
        "#VE virtualization",
        "#CP control protection",
    };

    if (vector < sizeof(names) / sizeof(names[0])) {
        return names[vector];
    }
    return "unknown";
}

static u64 read_cr2(void) {
    u64 value;
    __asm__ volatile ("mov %%cr2, %0" : "=r"(value));
    return value;
}

static void print_frame(const interrupt_frame_t *frame) {
    serial_write("interrupt: vector=");
    serial_write_hex64(frame->vector);
    serial_write(" ");
    serial_write(exception_name(frame->vector));
    serial_write("\n");

    serial_write("interrupt: error=");
    serial_write_hex64(frame->error_code);
    serial_write(" rip=");
    serial_write_hex64(frame->rip);
    serial_write(" cs=");
    serial_write_hex64(frame->cs);
    serial_write(" rflags=");
    serial_write_hex64(frame->rflags);
    serial_write("\n");

    if ((frame->cs & 3u) == 3u) {
        serial_write("interrupt: rsp=");
        serial_write_hex64(frame->rsp);
        serial_write(" ss=");
        serial_write_hex64(frame->ss);
        serial_write("\n");
    }

    serial_write("interrupt: rax="); serial_write_hex64(frame->rax);
    serial_write(" rbx="); serial_write_hex64(frame->rbx);
    serial_write(" rcx="); serial_write_hex64(frame->rcx);
    serial_write(" rdx="); serial_write_hex64(frame->rdx);
    serial_write("\n");

    serial_write("interrupt: rsi="); serial_write_hex64(frame->rsi);
    serial_write(" rdi="); serial_write_hex64(frame->rdi);
    serial_write(" rbp="); serial_write_hex64(frame->rbp);
    serial_write(" r8="); serial_write_hex64(frame->r8);
    serial_write("\n");

    serial_write("interrupt: r9="); serial_write_hex64(frame->r9);
    serial_write(" r10="); serial_write_hex64(frame->r10);
    serial_write(" r11="); serial_write_hex64(frame->r11);
    serial_write(" r12="); serial_write_hex64(frame->r12);
    serial_write("\n");

    serial_write("interrupt: r13="); serial_write_hex64(frame->r13);
    serial_write(" r14="); serial_write_hex64(frame->r14);
    serial_write(" r15="); serial_write_hex64(frame->r15);
    serial_write("\n");

    if (frame->vector == 14) {
        serial_write_line_hex("interrupt: cr2=", read_cr2());
    }
}

static void halt_forever(void) {
    for (;;) {
        __asm__ volatile ("cli; hlt");
    }
}

static void handle_exception(const interrupt_frame_t *frame, bool halt) {
    print_frame(frame);
    if (halt) {
        serial_write("interrupt: fatal exception, halting\n");
        halt_forever();
    }
}

void exception_demo_showcase(void) {
    interrupt_frame_t demo;
    k_memset(&demo, 0, sizeof(demo));
    demo.vector = 14;
    demo.error_code = 0x2;
    demo.rip = 0x0000000000bad000ULL;
    demo.cs = GDT_USER_CODE | 3u;
    demo.rflags = 0x202;
    demo.rsp = 0x0000000000caf000ULL;
    demo.ss = GDT_USER_DATA | 3u;

    serial_write("kernel: exception demo follows\n");
    handle_exception(&demo, false);
}

void arch_interrupt_dispatch(interrupt_frame_t *frame) {
    if (frame->vector == 32) {
        pit_handle_irq();
#if CONFIG_SCHEDULER
        scheduler_on_timer(frame);
#endif
        pic_send_eoi(0);
        return;
    }

    if (frame->vector == 33) {
        keyboard_handle_irq();
        pic_send_eoi(1);
        return;
    }

    if (frame->vector < 32) {
        handle_exception(frame, true);
        return;
    }

    serial_write("interrupt: unexpected vector ");
    serial_write_hex64(frame->vector);
    serial_write("\n");
    halt_forever();
}
