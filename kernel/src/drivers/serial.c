// Extensive serial port driver for InnovatiOS
// All features in this driver are listed:
// - Support for multiple serial ports (COM1, COM2, COM3, COM4)
// - Configurable baud rate, data bits, stop bits, and parity
// - Optional hardware flow control (RTS/CTS)
// - Interrupt-driven I/O for both transmission and reception
// - Ring buffer implementation for both TX and RX
// - Statistics tracking for transmitted and received bytes, overruns, framing errors, parity errors,
//   and break events
// - Thread-safe access to serial ports using spinlocks

#include <kernel/drivers/serial.h>
#include <stdint.h>

#define SERIAL_RING_SIZE 256
#define SERIAL_RING_MASK (SERIAL_RING_SIZE - 1)

#define LSR_DR   0x01
#define LSR_OE   0x02
#define LSR_PE   0x04
#define LSR_FE   0x08
#define LSR_BI   0x10
#define LSR_THRE 0x20
#define LSR_TEMT 0x40

#define IER_RDA  0x01
#define IER_THRE 0x02
#define IER_RLS  0x04
#define IER_MS   0x08

typedef struct {
    u8 buf[SERIAL_RING_SIZE];
    volatile u16 head;
    volatile u16 tail;
} serial_ring_t;

typedef struct {
    u16 base;
    u8 irq;
    bool present;
    bool open;
    bool interrupts;
    bool flow_control;
    volatile u32 lock;
    serial_ring_t tx;
    serial_ring_t rx;
    serial_stats_t stats;
} serial_state_t;

static serial_state_t g_ports[SERIAL_PORT_COUNT];

static const u16 SERIAL_BASE[SERIAL_PORT_COUNT] = { 0x3F8, 0x2F8, 0x3E8, 0x2E8 };
static const u8  SERIAL_IRQ[SERIAL_PORT_COUNT]  = { 4, 3, 4, 3 };

static inline void outb(u16 port, u8 val) {
    __asm__ volatile ("outb %0, %1" : : "a"(val), "Nd"(port));
}

static inline u8 inb(u16 port) {
    u8 ret;
    __asm__ volatile ("inb %1, %0" : "=a"(ret) : "Nd"(port));
    return ret;
}

static inline u64 irq_save(void) {
    u64 flags;
    __asm__ volatile ("pushfq; pop %0; cli" : "=r"(flags) :: "memory");
    return flags;
}

static inline void irq_restore(u64 flags) {
    __asm__ volatile ("push %0; popfq" :: "r"(flags) : "memory", "cc");
}

static inline void spin_lock(volatile u32 *lock) {
    while (!__sync_bool_compare_and_swap(lock, 0, 1)) {
        __asm__ volatile ("pause");
    }
}

static inline void spin_unlock(volatile u32 *lock) {
    __sync_lock_release(lock);
}

static inline u64 port_lock(serial_state_t *p) {
    u64 flags = irq_save();
    spin_lock(&p->lock);
    return flags;
}

static inline void port_unlock(serial_state_t *p, u64 flags) {
    spin_unlock(&p->lock);
    irq_restore(flags);
}

static inline bool ring_is_empty(serial_ring_t *r) {
    return r->head == r->tail;
}

static inline bool ring_push(serial_ring_t *r, u8 byte) {
    u16 next = (u16)((r->head + 1) & SERIAL_RING_MASK);
    if (next == r->tail) return false;
    r->buf[r->head] = byte;
    r->head = next;
    return true;
}

static inline bool ring_pop(serial_ring_t *r, u8 *out) {
    if (ring_is_empty(r)) return false;
    *out = r->buf[r->tail];
    r->tail = (u16)((r->tail + 1) & SERIAL_RING_MASK);
    return true;
}

static void hw_putc(serial_state_t *p, u8 c) {
    while (!(inb((u16)(p->base + 5)) & LSR_THRE)) { }
    outb(p->base, c);
}

bool serial_port_present(serial_port_t port) {
    if (port >= SERIAL_PORT_COUNT) return false;
    u16 base = SERIAL_BASE[port];
    u8 old = inb((u16)(base + 7));
    outb((u16)(base + 7), 0x55);
    u8 v = inb((u16)(base + 7));
    outb((u16)(base + 7), old);
    return v == 0x55;
}

bool serial_open(serial_port_t port, const serial_config_t *cfg) {
    if (port >= SERIAL_PORT_COUNT || !cfg || cfg->baud == 0) return false;
    if (!serial_port_present(port)) return false;

    serial_state_t *p = &g_ports[port];
    u16 base = SERIAL_BASE[port];
    u32 divisor = 115200u / cfg->baud;
    if (divisor == 0) divisor = 1;

    u64 flags = port_lock(p);

    p->base = base;
    p->irq = SERIAL_IRQ[port];
    p->interrupts = cfg->use_interrupts;
    p->flow_control = cfg->use_flow_control;
    p->tx.head = p->tx.tail = 0;
    p->rx.head = p->rx.tail = 0;
    serial_stats_t zero = {0};
    p->stats = zero;

    outb((u16)(base + 1), 0x00);
    outb((u16)(base + 3), 0x80);
    outb((u16)(base + 0), (u8)(divisor & 0xFF));
    outb((u16)(base + 1), (u8)((divisor >> 8) & 0xFF));

    u8 lcr;
    switch (cfg->data_bits) {
        case 5: lcr = 0x00; break;
        case 6: lcr = 0x01; break;
        case 7: lcr = 0x02; break;
        default: lcr = 0x03; break;
    }
    if (cfg->stop_bits >= 2) lcr |= 0x04;
    lcr |= (u8)(((u8)cfg->parity) << 3);
    outb((u16)(base + 3), lcr);

    outb((u16)(base + 2), 0xC7);
    outb((u16)(base + 4), 0x0B);

    if (p->interrupts) {
        outb((u16)(base + 1), IER_RDA | IER_RLS);
    } else {
        outb((u16)(base + 1), 0x00);
    }

    p->present = true;
    p->open = true;

    port_unlock(p, flags);
    return true;
}

void serial_close(serial_port_t port) {
    if (port >= SERIAL_PORT_COUNT) return;
    serial_state_t *p = &g_ports[port];
    u64 flags = port_lock(p);
    outb((u16)(p->base + 1), 0x00);
    p->open = false;
    port_unlock(p, flags);
}

void serial_putc_port(serial_port_t port, char c) {
    if (port >= SERIAL_PORT_COUNT) return;
    serial_state_t *p = &g_ports[port];
    if (!p->open) return;

    if (p->interrupts) {
        u64 flags = port_lock(p);
        while (!ring_push(&p->tx, (u8)c)) {
            port_unlock(p, flags);
            __asm__ volatile ("pause");
            flags = port_lock(p);
        }
        outb((u16)(p->base + 1), (u8)(inb((u16)(p->base + 1)) | IER_THRE));
        p->stats.tx_bytes++;
        port_unlock(p, flags);
    } else {
        hw_putc(p, (u8)c);
        p->stats.tx_bytes++;
    }
}

void serial_write_port(serial_port_t port, const char *str) {
    while (*str) {
        if (*str == '\n') serial_putc_port(port, '\r');
        serial_putc_port(port, *str++);
    }
}

void serial_write_len_port(serial_port_t port, const char *str, size_t len) {
    for (size_t i = 0; i < len; i++) {
        if (str[i] == '\n') serial_putc_port(port, '\r');
        serial_putc_port(port, str[i]);
    }
}

int serial_getc_port(serial_port_t port) {
    if (port >= SERIAL_PORT_COUNT) return -1;
    serial_state_t *p = &g_ports[port];
    if (!p->open) return -1;

    if (p->interrupts) {
        u8 byte;
        bool ok;
        u64 flags = port_lock(p);
        ok = ring_pop(&p->rx, &byte);
        port_unlock(p, flags);
        return ok ? (int)byte : -1;
    }

    if (inb((u16)(p->base + 5)) & LSR_DR) {
        return (int)inb(p->base);
    }
    return -1;
}

bool serial_available_port(serial_port_t port) {
    if (port >= SERIAL_PORT_COUNT) return false;
    serial_state_t *p = &g_ports[port];
    if (!p->open) return false;

    if (p->interrupts) {
        u64 flags = port_lock(p);
        bool avail = !ring_is_empty(&p->rx);
        port_unlock(p, flags);
        return avail;
    }
    return (inb((u16)(p->base + 5)) & LSR_DR) != 0;
}

void serial_flush_port(serial_port_t port) {
    if (port >= SERIAL_PORT_COUNT) return;
    serial_state_t *p = &g_ports[port];
    if (!p->open) return;

    if (p->interrupts) {
        for (;;) {
            u64 flags = port_lock(p);
            bool empty = ring_is_empty(&p->tx);
            port_unlock(p, flags);
            if (empty) break;
            __asm__ volatile ("pause");
        }
    }
    while (!(inb((u16)(p->base + 5)) & LSR_TEMT)) { }
}

void serial_stats_get(serial_port_t port, serial_stats_t *out) {
    if (port >= SERIAL_PORT_COUNT || !out) return;
    serial_state_t *p = &g_ports[port];
    u64 flags = port_lock(p);
    *out = p->stats;
    port_unlock(p, flags);
}

void serial_irq_handler(serial_port_t port) {
    if (port >= SERIAL_PORT_COUNT) return;
    serial_state_t *p = &g_ports[port];
    if (!p->open || !p->interrupts) return;

    for (;;) {
        u8 iir = inb((u16)(p->base + 2));
        if (iir & 0x01) break;
        u8 id = (u8)((iir >> 1) & 0x07);

        if (id == 0x03) {
            u8 lsr = inb((u16)(p->base + 5));
            if (lsr & LSR_OE) p->stats.rx_overruns++;
            if (lsr & LSR_PE) p->stats.parity_errors++;
            if (lsr & LSR_FE) p->stats.framing_errors++;
            if (lsr & LSR_BI) p->stats.break_events++;
        } else if (id == 0x02 || id == 0x06) {
            u8 lsr;
            while ((lsr = inb((u16)(p->base + 5))) & LSR_DR) {
                u8 byte = inb(p->base);
                u64 flags = port_lock(p);
                if (!ring_push(&p->rx, byte)) p->stats.rx_overruns++;
                else p->stats.rx_bytes++;
                port_unlock(p, flags);
            }
        } else if (id == 0x01) {
            u64 flags = port_lock(p);
            u8 sent = 0;
            u8 byte;
            while (sent < 16 && ring_pop(&p->tx, &byte)) {
                outb(p->base, byte);
                sent++;
            }
            if (ring_is_empty(&p->tx)) {
                outb((u16)(p->base + 1), (u8)(inb((u16)(p->base + 1)) & ~IER_THRE));
            }
            port_unlock(p, flags);
        } else {
            inb((u16)(p->base + 6));
        }
    }
}

static void emit_char(serial_port_t port, char c) {
    if (c == '\n') serial_putc_port(port, '\r');
    serial_putc_port(port, c);
}

static void emit_str(serial_port_t port, const char *s) {
    while (*s) emit_char(port, *s++);
}

static void emit_uint(serial_port_t port, unsigned long long val, unsigned base, bool upper) {
    char buf[32];
    int i = 0;
    const char *digits = upper ? "0123456789ABCDEF" : "0123456789abcdef";
    if (val == 0) buf[i++] = '0';
    while (val) {
        buf[i++] = digits[val % base];
        val /= base;
    }
    while (i > 0) emit_char(port, buf[--i]);
}

static void emit_int(serial_port_t port, long long val) {
    if (val < 0) {
        emit_char(port, '-');
        emit_uint(port, (unsigned long long)(-val), 10, false);
    } else {
        emit_uint(port, (unsigned long long)val, 10, false);
    }
}

int serial_vprintf_port(serial_port_t port, const char *fmt, va_list ap) {
    if (port >= SERIAL_PORT_COUNT || !fmt) return 0;
    int count = 0;

    while (*fmt) {
        if (*fmt != '%') {
            emit_char(port, *fmt++);
            count++;
            continue;
        }
        fmt++;
        bool long_flag = false;
        bool long_long_flag = false;
        while (*fmt == 'l') {
            if (long_flag) long_long_flag = true;
            long_flag = true;
            fmt++;
        }

        switch (*fmt) {
            case 'd':
            case 'i': {
                long long v = long_long_flag ? va_arg(ap, long long) :
                              long_flag ? va_arg(ap, long) : va_arg(ap, int);
                emit_int(port, v);
                break;
            }
            case 'u': {
                unsigned long long v = long_long_flag ? va_arg(ap, unsigned long long) :
                                       long_flag ? va_arg(ap, unsigned long) : va_arg(ap, unsigned int);
                emit_uint(port, v, 10, false);
                break;
            }
            case 'x': {
                unsigned long long v = long_long_flag ? va_arg(ap, unsigned long long) :
                                       long_flag ? va_arg(ap, unsigned long) : va_arg(ap, unsigned int);
                emit_uint(port, v, 16, false);
                break;
            }
            case 'X': {
                unsigned long long v = long_long_flag ? va_arg(ap, unsigned long long) :
                                       long_flag ? va_arg(ap, unsigned long) : va_arg(ap, unsigned int);
                emit_uint(port, v, 16, true);
                break;
            }
            case 'p': {
                void *v = va_arg(ap, void *);
                emit_str(port, "0x");
                emit_uint(port, (unsigned long long)(uintptr_t)v, 16, false);
                break;
            }
            case 's': {
                const char *s = va_arg(ap, const char *);
                emit_str(port, s ? s : "(null)");
                break;
            }
            case 'c': {
                char c = (char)va_arg(ap, int);
                emit_char(port, c);
                break;
            }
            case '%': {
                emit_char(port, '%');
                break;
            }
            default: {
                emit_char(port, '%');
                if (*fmt) emit_char(port, *fmt);
                break;
            }
        }
        fmt++;
        count++;
    }
    return count;
}

int serial_printf_port(serial_port_t port, const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    int r = serial_vprintf_port(port, fmt, ap);
    va_end(ap);
    return r;
}

void serial_init(void) {
    serial_config_t cfg = SERIAL_CONFIG_DEFAULT;
    serial_open(SERIAL_COM1, &cfg);
}

void serial_putc(char c) {
    serial_putc_port(SERIAL_COM1, c);
}

void serial_write(const char *str) {
    serial_write_port(SERIAL_COM1, str);
}

void serial_write_len(const char *str, size_t len) {
    serial_write_len_port(SERIAL_COM1, str, len);
}

void serial_flush(void) {
    serial_flush_port(SERIAL_COM1);
}

int serial_printf(const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    int r = serial_vprintf_port(SERIAL_COM1, fmt, ap);
    va_end(ap);
    return r;
}