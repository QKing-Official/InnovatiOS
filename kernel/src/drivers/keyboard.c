#include <kernel/drivers/keyboard.h>
#include <kernel/arch/x86_64/io.h>
#include <kernel/arch/x86_64/pic.h>

#define KB_DATA_PORT 0x60
#define KB_BUFFER_SIZE 256

static volatile char kb_buffer[KB_BUFFER_SIZE];
static volatile u32 kb_head = 0;
static volatile u32 kb_tail = 0;

static volatile int shift_held = 0;

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

#define SC_LSHIFT       0x2A
#define SC_RSHIFT       0x36
#define SC_LSHIFT_BREAK 0xAA
#define SC_RSHIFT_BREAK 0xB6

static void kb_buffer_push(char c) {
    u32 next = (kb_head + 1) % KB_BUFFER_SIZE;
    if (next == kb_tail) return;
    kb_buffer[kb_head] = c;
    kb_head = next;
}

int keyboard_has_char(void) {
    return kb_head != kb_tail;
}

char keyboard_read_char(void) {
    while (kb_head == kb_tail) {
        __asm__ volatile ("hlt");
    }
    char c = kb_buffer[kb_tail];
    kb_tail = (kb_tail + 1) % KB_BUFFER_SIZE;
    return c;
}

void keyboard_handle_irq(void) {
    u8 sc = inb(KB_DATA_PORT);

    if (sc == SC_LSHIFT || sc == SC_RSHIFT) {
        shift_held = 1;
    } else if (sc == SC_LSHIFT_BREAK || sc == SC_RSHIFT_BREAK) {
        shift_held = 0;
    } else if (!(sc & 0x80)) {
        char c = shift_held ? scancode_ascii_shift[sc] : scancode_ascii[sc];
        if (c != 0) {
            kb_buffer_push(c);
        }
    }
}

void keyboard_init(void) {
    pic_unmask_irq(1);
}
