#ifndef KERNEL_DRIVERS_SERIAL_H
#define KERNEL_DRIVERS_SERIAL_H

#include <kernel/types.h>
#include <stdarg.h>
#include <stdbool.h>

typedef enum {
    SERIAL_COM1 = 0,
    SERIAL_COM2 = 1,
    SERIAL_COM3 = 2,
    SERIAL_COM4 = 3,
    SERIAL_PORT_COUNT
} serial_port_t;

typedef enum {
    SERIAL_PARITY_NONE  = 0,
    SERIAL_PARITY_ODD   = 1,
    SERIAL_PARITY_EVEN  = 3,
    SERIAL_PARITY_MARK  = 5,
    SERIAL_PARITY_SPACE = 7
} serial_parity_t;

typedef struct {
    u32 baud;
    u8 data_bits;
    u8 stop_bits;
    serial_parity_t parity;
    bool use_interrupts;
    bool use_flow_control;
} serial_config_t;

typedef struct {
    u32 tx_bytes;
    u32 rx_bytes;
    u32 rx_overruns;
    u32 framing_errors;
    u32 parity_errors;
    u32 break_events;
} serial_stats_t;

#define SERIAL_CONFIG_DEFAULT { .baud = 115200, .data_bits = 8, .stop_bits = 1, .parity = SERIAL_PARITY_NONE, .use_interrupts = false, .use_flow_control = false }

#if CONFIG_SERIAL

bool serial_port_present(serial_port_t port);
bool serial_open(serial_port_t port, const serial_config_t *cfg);
void serial_close(serial_port_t port);

void serial_putc_port(serial_port_t port, char c);
void serial_write_port(serial_port_t port, const char *str);
void serial_write_len_port(serial_port_t port, const char *str, size_t len);
int  serial_getc_port(serial_port_t port);
bool serial_available_port(serial_port_t port);
void serial_flush_port(serial_port_t port);
void serial_stats_get(serial_port_t port, serial_stats_t *out);
void serial_irq_handler(serial_port_t port);
int  serial_vprintf_port(serial_port_t port, const char *fmt, va_list ap);
int  serial_printf_port(serial_port_t port, const char *fmt, ...);

void serial_init(void);
void serial_putc(char c);
void serial_write(const char *str);
void serial_write_len(const char *str, size_t len);
void serial_flush(void);
int  serial_printf(const char *fmt, ...);

#else

static inline bool serial_port_present(serial_port_t port) { (void)port; return false; }
static inline bool serial_open(serial_port_t port, const serial_config_t *cfg) { (void)port; (void)cfg; return false; }
static inline void serial_close(serial_port_t port) { (void)port; }
static inline void serial_putc_port(serial_port_t port, char c) { (void)port; (void)c; }
static inline void serial_write_port(serial_port_t port, const char *str) { (void)port; (void)str; }
static inline void serial_write_len_port(serial_port_t port, const char *str, size_t len) { (void)port; (void)str; (void)len; }
static inline int serial_getc_port(serial_port_t port) { (void)port; return -1; }
static inline bool serial_available_port(serial_port_t port) { (void)port; return false; }
static inline void serial_flush_port(serial_port_t port) { (void)port; }
static inline void serial_stats_get(serial_port_t port, serial_stats_t *out) { (void)port; if (out) { serial_stats_t z = {0}; *out = z; } }
static inline void serial_irq_handler(serial_port_t port) { (void)port; }
static inline int serial_vprintf_port(serial_port_t port, const char *fmt, va_list ap) { (void)port; (void)fmt; (void)ap; return 0; }
static inline int serial_printf_port(serial_port_t port, const char *fmt, ...) { (void)port; (void)fmt; return 0; }

static inline void serial_init(void) { }
static inline void serial_putc(char c) { (void)c; }
static inline void serial_write(const char *str) { (void)str; }
static inline void serial_write_len(const char *str, size_t len) { (void)str; (void)len; }
static inline void serial_flush(void) { }
static inline int serial_printf(const char *fmt, ...) { (void)fmt; return 0; }

#endif

#endif