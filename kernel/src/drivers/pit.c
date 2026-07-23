// The PIT (Programmable Interval Timer) driver for InnovatiOS
#include <kernel/drivers/pit.h>
#include <kernel/arch/x86_64/io.h>

// PIT channels
#define PIT_CHANNEL0 0x40
#define PIT_COMMAND  0x43
#define PIT_BASE_HZ  1193182u

// Global tick counter and frequency
static volatile u64 g_ticks = 0;
static u32 g_hz = 100;

void pit_init(u32 hz) {
    g_hz = hz;
    u32 divisor = PIT_BASE_HZ / hz;

    outb(PIT_COMMAND, 0x36);
    outb(PIT_CHANNEL0, (u8)(divisor & 0xFF));
    outb(PIT_CHANNEL0, (u8)((divisor >> 8) & 0xFF));

    g_ticks = 0;
}

// Get the current tick count
u64 pit_get_ticks(void) {
    return g_ticks;
}

// Sleep for a specified number of milliseconds
void pit_sleep_ms(u32 ms) {
    u64 target = g_ticks + ((u64)ms * g_hz) / 1000;
    while (g_ticks < target) {
        __asm__ volatile ("hlt");
    }
}

// IRQ handler for the PIT
void pit_handle_irq(void) {
    g_ticks++;
}
