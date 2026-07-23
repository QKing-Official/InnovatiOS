#include <kernel/drivers/keyboard.h>
#include <kernel/arch/x86_64/io.h>
#include <kernel/arch/x86_64/pic.h>


// Port to use for keyboard in hex
#define KB_DATA_PORT 0x60
#define KB_BUFFER_SIZE 256

static volatile keycode_t kb_buffer[KB_BUFFER_SIZE];
static volatile u32 kb_head = 0;
static volatile u32 kb_tail = 0;

static volatile int shift_held = 0;
static volatile int ctrl_held  = 0;
static volatile int alt_held   = 0;
static volatile int gui_held   = 0;

static volatile int capslock_state  = 0;
static volatile int numlock_state   = 0;
static volatile int scrolllock_state = 0;

static volatile int extended = 0;

static volatile int pause_seq_remaining = 0;

// Scancodes for the ASCII characters and their capitalized version for shift/capslock

static const char scancode_ascii[128] = {
    0,   27,  '1', '2', '3', '4', '5', '6', '7', '8', '9', '0', '-', '=', '\b', '\t',
    'q', 'w', 'e', 'r', 't', 'y', 'u', 'i', 'o', 'p', '[', ']', '\n', 0,   'a', 's',
    'd', 'f', 'g', 'h', 'j', 'k', 'l', ';', '\'','`', 0,   '\\','z', 'x', 'c', 'v',
    'b', 'n', 'm', ',', '.', '/', 0,   '*', 0,   ' ', 0,   0,   0,   0,   0,   0,
    0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
    0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
    0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
    0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
};

// Shifted versions here....
static const char scancode_ascii_shift[128] = {
    0,   27,  '!', '@', '#', '$', '%', '^', '&', '*', '(', ')', '_', '+', '\b', '\t',
    'Q', 'W', 'E', 'R', 'T', 'Y', 'U', 'I', 'O', 'P', '{', '}', '\n', 0,   'A', 'S',
    'D', 'F', 'G', 'H', 'J', 'K', 'L', ':', '"', '~', 0,   '|', 'Z', 'X', 'C', 'V',
    'B', 'N', 'M', '<', '>', '?', 0,   '*', 0,   ' ', 0,   0,   0,   0,   0,   0,
    0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
    0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
    0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
    0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
};

#define NUMPAD_SC_BASE 0x47
#define NUMPAD_SC_LAST 0x53

// Numpad is weird
static const char numpad_ascii[NUMPAD_SC_LAST - NUMPAD_SC_BASE + 1] = {
    '7', '8', '9', '-', '4', '5', '6', '+', '1', '2', '3', '0', '.',
};
// Numpad for when numlock is off
static const keycode_t numpad_nav[NUMPAD_SC_LAST - NUMPAD_SC_BASE + 1] = {
    KEY_HOME, KEY_UP,   KEY_PAGE_UP, 0 ,
    KEY_LEFT, 0 , KEY_RIGHT,  0 ,
    KEY_END,  KEY_DOWN, KEY_PAGE_DOWN, KEY_INSERT, KEY_DELETE,
};

// Some hex keycoded that define the ASCII characters and key
#define SC_LSHIFT     0x2A
#define SC_RSHIFT     0x36
#define SC_LCTRL      0x1D
#define SC_LALT       0x38
#define SC_CAPSLOCK   0x3A
#define SC_NUMLOCK    0x45
#define SC_SCROLLLOCK 0x46

#define SC_F1  0x3B
#define SC_F10 0x44
#define SC_F11 0x57
#define SC_F12 0x58


#define SCE_RCTRL     0x1D
#define SCE_RALT      0x38
#define SCE_NUMENTER  0x1C
#define SCE_NUMDIV    0x35
#define SCE_LGUI      0x5B
#define SCE_RGUI      0x5C
#define SCE_MENU      0x5D
#define SCE_UP        0x48
#define SCE_DOWN      0x50
#define SCE_LEFT      0x4B
#define SCE_RIGHT     0x4D
#define SCE_HOME      0x47
#define SCE_END       0x4F
#define SCE_PAGEUP    0x49
#define SCE_PAGEDOWN  0x51
#define SCE_INSERT    0x52
#define SCE_DELETE    0x53
#define SCE_PRTSCN_B  0x37

#define BREAK_BIT 0x80

static void kb_buffer_push(keycode_t k) {
    u32 next = (kb_head + 1) % KB_BUFFER_SIZE;
    if (next == kb_tail) return;
    kb_buffer[kb_head] = k;
    kb_head = next;
}

int keyboard_has_key(void) {
    return kb_head != kb_tail;
}

keycode_t keyboard_read_key(void) {
    while (kb_head == kb_tail) {
        __asm__ volatile ("hlt");
    }
    keycode_t k = kb_buffer[kb_tail];
    kb_tail = (kb_tail + 1) % KB_BUFFER_SIZE;
    return k;
}

int keyboard_has_char(void) {
    return keyboard_has_key();
}

char keyboard_read_char(void) {
    keycode_t k = keyboard_read_key();
    return (k < KEY_SPECIAL_BASE) ? (char)k : 0;
}

u8 keyboard_get_modifiers(void) {
    u8 mods = 0;
    if (shift_held)       mods |= KB_MOD_SHIFT;
    if (ctrl_held)        mods |= KB_MOD_CTRL;
    if (alt_held)         mods |= KB_MOD_ALT;
    if (gui_held)         mods |= KB_MOD_GUI;
    if (capslock_state)   mods |= KB_MOD_CAPS;
    if (numlock_state)    mods |= KB_MOD_NUM;
    if (scrolllock_state) mods |= KB_MOD_SCRL;
    return mods;
}

// Translator for the keyboard special characters
static char translate_normal_key(u8 code) {
    char base = scancode_ascii[code];
    if (base == 0) return 0;

    int use_shift = shift_held;
    if (base >= 'a' && base <= 'z') {
        use_shift ^= capslock_state;
    }

    char c = use_shift ? scancode_ascii_shift[code] : base;

    if (ctrl_held) {
        if (c >= 'a' && c <= 'z') c = (char)(c - 'a' + 1);
        else if (c >= 'A' && c <= 'Z') c = (char)(c - 'A' + 1);
    }

    return c;
}

// Handle a normal scancode
static void handle_normal(u8 code, int is_break) {
    switch (code) {
        case SC_LSHIFT:
        case SC_RSHIFT:
            shift_held = !is_break;
            kb_buffer_push((keycode_t)(code == SC_LSHIFT ? KEY_LSHIFT : KEY_RSHIFT));
            return;

        case SC_LCTRL:
            ctrl_held = !is_break;
            return;

        case SC_LALT:
            alt_held = !is_break;
            return;

        case SC_CAPSLOCK:
            if (!is_break) capslock_state ^= 1;
            return;

        case SC_NUMLOCK:
            if (!is_break) numlock_state ^= 1;
            return;

        case SC_SCROLLLOCK:
            if (!is_break) scrolllock_state ^= 1;
            return;

        default:
            break;
    }

    if (!is_break && code >= SC_F1 && code <= SC_F10) {
        kb_buffer_push((keycode_t)(KEY_F1 + (code - SC_F1)));
        return;
    }
    if (!is_break && code == SC_F11) { kb_buffer_push(KEY_F11); return; }
    if (!is_break && code == SC_F12) { kb_buffer_push(KEY_F12); return; }

    if (code == 0x37) {
        if (!is_break) kb_buffer_push(KEY_NUMPAD_STAR);
        return;
    }

    if (code >= NUMPAD_SC_BASE && code <= NUMPAD_SC_LAST) {
        if (is_break) return;

        u8 idx = code - NUMPAD_SC_BASE;
        char digit = numpad_ascii[idx];
        keycode_t nav = numpad_nav[idx];

        if (digit == '-') {
            kb_buffer_push(KEY_NUMPAD_MINUS);
        } else if (digit == '+') {
            kb_buffer_push(KEY_NUMPAD_PLUS);
        } else if (numlock_state) {
            keycode_t k = (digit == '.') ? KEY_NUMPAD_DOT
                                          : (keycode_t)(KEY_NUMPAD_0 + (digit - '0'));
            kb_buffer_push(k);
        } else if (nav != 0) {
            kb_buffer_push(nav);
        }
        return;
    }

    if (is_break) return;

    char c = translate_normal_key(code);
    if (c != 0) {
        kb_buffer_push((keycode_t)c);
    }
}

// Extended scancode handling for special keys
static void handle_extended(u8 code, int is_break) {
    switch (code) {
        case SCE_RCTRL:
            ctrl_held = !is_break;
            return;
        case SCE_RALT:
            alt_held = !is_break;
            return;

        case SCE_NUMENTER:
            if (!is_break) kb_buffer_push(KEY_NUMPAD_ENTER);
            return;
        case SCE_NUMDIV:
            if (!is_break) kb_buffer_push(KEY_NUMPAD_SLASH);
            return;

        default:
            break;
    }

    if (is_break) return;

    switch (code) {
        case SCE_UP:       kb_buffer_push(KEY_UP); return;
        case SCE_DOWN:      kb_buffer_push(KEY_DOWN); return;
        case SCE_LEFT:      kb_buffer_push(KEY_LEFT); return;
        case SCE_RIGHT:     kb_buffer_push(KEY_RIGHT); return;
        case SCE_HOME:      kb_buffer_push(KEY_HOME); return;
        case SCE_END:       kb_buffer_push(KEY_END); return;
        case SCE_PAGEUP:    kb_buffer_push(KEY_PAGE_UP); return;
        case SCE_PAGEDOWN:  kb_buffer_push(KEY_PAGE_DOWN); return;
        case SCE_INSERT:    kb_buffer_push(KEY_INSERT); return;
        case SCE_DELETE:    kb_buffer_push(KEY_DELETE); return;
        case SCE_LGUI:      gui_held = 1; kb_buffer_push(KEY_LGUI); return;
        case SCE_RGUI:      gui_held = 1; kb_buffer_push(KEY_RGUI); return;
        case SCE_MENU:      kb_buffer_push(KEY_MENU); return;
        case SCE_PRTSCN_B:  kb_buffer_push(KEY_PRINTSCREEN); return;
        default:            return;
    }
}

// Interrupt handler for the keyboard IRQ
void keyboard_handle_irq(void) {
    u8 sc = inb(KB_DATA_PORT);

    if (pause_seq_remaining > 0) {
        pause_seq_remaining--;
        if (pause_seq_remaining == 0) {
            kb_buffer_push(KEY_PAUSE);
        }
        return;
    }

    if (sc == 0xE1) {
        pause_seq_remaining = 5;
        return;
    }

    if (sc == 0xE0) {
        extended = 1;
        return;
    }

    int is_break = (sc & BREAK_BIT) != 0;
    u8 code = sc & 0x7F;
    int was_extended = extended;
    extended = 0;

    if (was_extended) {
        handle_extended(code, is_break);
    } else {
        handle_normal(code, is_break);
    }
}

void keyboard_init(void) {
    pic_unmask_irq(1);
}