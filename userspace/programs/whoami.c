#include <programs.h>
#include <kernel/drivers/console.h>

void prog_whoami(user_t *user, const char *args) {
    (void)args;
    console_puts(user->username);
    if (user->is_admin) {
        console_puts_color(" (admin)", CON_COLOR_YELLOW);
    }
    console_puts("\n");
}

