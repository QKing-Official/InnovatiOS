#ifndef KERNEL_DRIVERS_SERIAL_H
#define KERNEL_DRIVERS_SERIAL_H

#include <kernel/types.h>

#if CONFIG_SERIAL
void serial_init(void);
void serial_putc(char c);
void serial_write(const char *str);
void serial_write_len(const char *str, size_t len);
#else
static inline void serial_init(void) { }
static inline void serial_putc(char c) { (void)c; }
static inline void serial_write(const char *str) { (void)str; }
static inline void serial_write_len(const char *str, size_t len) { (void)str; (void)len; }
#endif

#endif

