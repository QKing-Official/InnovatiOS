#ifndef KERNEL_DRIVERS_KEYBOARD_H
#define KERNEL_DRIVERS_KEYBOARD_H

#include <kernel/types.h>

typedef u16 keycode_t;

#define KEY_SPECIAL_BASE 0x0100

enum {
    KEY_UP = KEY_SPECIAL_BASE,
    KEY_DOWN,
    KEY_LEFT,
    KEY_RIGHT,

    KEY_HOME,
    KEY_END,
    KEY_PAGE_UP,
    KEY_PAGE_DOWN,
    KEY_INSERT,
    KEY_DELETE,

    KEY_F1, KEY_F2, KEY_F3, KEY_F4, KEY_F5, KEY_F6,
    KEY_F7, KEY_F8, KEY_F9, KEY_F10, KEY_F11, KEY_F12,

    KEY_LCTRL, KEY_RCTRL,
    KEY_LALT,  KEY_RALT,
    KEY_LSHIFT, KEY_RSHIFT,
    KEY_LGUI,  KEY_RGUI,
    KEY_MENU,

    KEY_CAPSLOCK,
    KEY_NUMLOCK,
    KEY_SCROLLLOCK,

    KEY_PRINTSCREEN,
    KEY_PAUSE,

    KEY_NUMPAD_0, KEY_NUMPAD_1, KEY_NUMPAD_2, KEY_NUMPAD_3, KEY_NUMPAD_4,
    KEY_NUMPAD_5, KEY_NUMPAD_6, KEY_NUMPAD_7, KEY_NUMPAD_8, KEY_NUMPAD_9,
    KEY_NUMPAD_DOT,
    KEY_NUMPAD_PLUS, KEY_NUMPAD_MINUS, KEY_NUMPAD_STAR, KEY_NUMPAD_SLASH,
    KEY_NUMPAD_ENTER,
};

static inline char keycode_to_ascii(keycode_t k) {
    if (k < KEY_SPECIAL_BASE) return (char)k;

    switch (k) {
        case KEY_NUMPAD_0: return '0';
        case KEY_NUMPAD_1: return '1';
        case KEY_NUMPAD_2: return '2';
        case KEY_NUMPAD_3: return '3';
        case KEY_NUMPAD_4: return '4';
        case KEY_NUMPAD_5: return '5';
        case KEY_NUMPAD_6: return '6';
        case KEY_NUMPAD_7: return '7';
        case KEY_NUMPAD_8: return '8';
        case KEY_NUMPAD_9: return '9';
        case KEY_NUMPAD_DOT:   return '.';
        case KEY_NUMPAD_PLUS:  return '+';
        case KEY_NUMPAD_MINUS: return '-';
        case KEY_NUMPAD_STAR:  return '*';
        case KEY_NUMPAD_SLASH: return '/';
        case KEY_NUMPAD_ENTER: return '\n';
        default: return 0;
    }
}

#define KB_MOD_SHIFT (1 << 0)
#define KB_MOD_CTRL  (1 << 1)
#define KB_MOD_ALT   (1 << 2)
#define KB_MOD_GUI   (1 << 3)
#define KB_MOD_CAPS  (1 << 4)
#define KB_MOD_NUM   (1 << 5) 
#define KB_MOD_SCRL  (1 << 6)

#if CONFIG_KEYBOARD

void keyboard_init(void);
void keyboard_handle_irq(void);

int  keyboard_has_char(void);
char keyboard_read_char(void);

int       keyboard_has_key(void);
keycode_t keyboard_read_key(void);

u8 keyboard_get_modifiers(void);

#else

static inline void keyboard_init(void) { }
static inline void keyboard_handle_irq(void) { }
static inline int  keyboard_has_char(void) { return 0; }
static inline char keyboard_read_char(void) { return 0; }
static inline int  keyboard_has_key(void) { return 0; }
static inline keycode_t keyboard_read_key(void) { return 0; }
static inline u8   keyboard_get_modifiers(void) { return 0; }

#endif

#endif