#ifndef KERNEL_DRIVERS_KEYBOARD_H
#define KERNEL_DRIVERS_KEYBOARD_H

#include <kernel/types.h>

#if CONFIG_KEYBOARD
void keyboard_init(void);
void keyboard_handle_irq(void);
int keyboard_has_char(void);
char keyboard_read_char(void);
#else
static inline void keyboard_init(void) { }
static inline void keyboard_handle_irq(void) { }
static inline int keyboard_has_char(void) { return 0; }
static inline char keyboard_read_char(void) { return 0; }
#endif

#endif
