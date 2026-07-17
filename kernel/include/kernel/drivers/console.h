#ifndef KERNEL_DRIVERS_CONSOLE_H
#define KERNEL_DRIVERS_CONSOLE_H

#include <kernel/types.h>

#define CON_COLOR_BG      0x00101014
#define CON_COLOR_WHITE   0x00E0E0E0
#define CON_COLOR_GREEN   0x0000FF88
#define CON_COLOR_RED     0x00FF4444
#define CON_COLOR_YELLOW  0x00FFD866
#define CON_COLOR_CYAN    0x0066D9EF
#define CON_COLOR_GREY    0x00888888
#define CON_COLOR_BLUE    0x005B6EE1

void console_init(void);
void console_set_color(u32 fg, u32 bg);
void console_putc(char c);
void console_puts(const char *s);
void console_clear(void);
void console_set_cursor(u64 col, u64 row);

void console_puts_color(const char *s, u32 fg);

void console_readline(char *buf, size_t len, char echo_mask);

#endif

