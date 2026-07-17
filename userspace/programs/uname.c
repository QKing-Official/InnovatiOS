#include <programs.h>
#include <kernel/drivers/console.h>

void prog_uname(user_t *user, const char *args) {
    (void)user;
    (void)args;
    console_puts_color("InnovatiOS", CON_COLOR_CYAN);
    console_puts(" v0.1 (x86_64)\n");
}

