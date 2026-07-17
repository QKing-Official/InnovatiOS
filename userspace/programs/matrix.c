#include <programs.h>
#include <kernel/drivers/console.h>

#if CONFIG_PIT
#include <kernel/drivers/pit.h>
#endif

#if CONFIG_KEYBOARD
#include <kernel/drivers/keyboard.h>
#endif

static u32 m_rand_state = 123456789;

static u32 m_rand(void) {
    m_rand_state = m_rand_state * 1103515245 + 12345;
    return (m_rand_state / 65536) % 32768;
}

void prog_matrix(user_t *user, const char *args) {
    (void)user;
    (void)args;

    console_clear();
    
    while (1) {
#if CONFIG_KEYBOARD
        if (keyboard_has_char()) {
            // clear the key from the buffer
            keyboard_read_char();
            break;
        }
#endif

        int spaces = m_rand() % 4;
        for (int i = 0; i < spaces; i++) {
            console_putc(' ');
        }
        
        char c = (m_rand() % (126 - 33)) + 33;
        
        // Randomly make some characters white or different shades if we had them
        if ((m_rand() % 10) == 0) {
            console_set_color(CON_COLOR_WHITE, CON_COLOR_BG);
        } else {
            console_set_color(CON_COLOR_GREEN, CON_COLOR_BG);
        }
        
        console_putc(c);

#if CONFIG_PIT
        pit_sleep_ms(2);
#endif
    }
    
    console_set_color(CON_COLOR_WHITE, CON_COLOR_BG);
    console_clear();
}
