#include <programs.h>
#include <kernel/drivers/console.h>
#include <kernel/types.h>

static inline void outb(u16 port, u8 value)
{
    __asm__ volatile ("outb %0, %1" : : "a"(value), "Nd"(port));
}

static inline u8 inb(u16 port)
{
    u8 value;
    __asm__ volatile ("inb %1, %0" : "=a"(value) : "Nd"(port));
    return value;
}

static void cpu_halt_forever(void)
{
    for (;;) {
        __asm__ volatile ("hlt");
    }
}

static void triple_fault_reset(void)
{
    struct {
        u16 limit;
        u64 base;
    } __attribute__((packed)) null_idt = {
        .limit = 0,
        .base  = 0
    };

    __asm__ volatile ("cli");
    __asm__ volatile ("lidt %0" : : "m"(null_idt));

    __asm__ volatile ("ud2");

    cpu_halt_forever();
}

void prog_reboot(user_t *user, const char *args)
{
    (void)user;
    (void)args;

    console_puts_color("Rebooting...\n", CON_COLOR_YELLOW);

    __asm__ volatile ("cli");

    outb(0xCF9, 0x02);

    for (volatile u32 i = 0; i < 100000; i++) {
        __asm__ volatile ("" ::: "memory");
    }

    outb(0xCF9, 0x06);

    for (volatile u32 i = 0; i < 1000000; i++) {
        __asm__ volatile ("" ::: "memory");
    }

    for (u32 i = 0; i < 100000; i++) {
        if ((inb(0x64) & 0x02) == 0) {
            break;
        }
    }

    outb(0x64, 0xFE);

    for (volatile u32 i = 0; i < 1000000; i++) {
        __asm__ volatile ("" ::: "memory");
    }

    triple_fault_reset();
}