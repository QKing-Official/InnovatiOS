#include <programs.h>
#include <kernel/drivers/console.h>
#include <kernel/lib/string.h>

void prog_adduser(user_t *user, const char *args) {
    if (!user->is_admin) {
        console_puts_color("Permission denied.\n", CON_COLOR_RED);
        return;
    }

    char name[USER_NAME_MAX];
    char pass[USER_NAME_MAX];
    args = prog_next_word(args, name, sizeof(name));
    args = prog_next_word(args, pass, sizeof(pass));

    if (name[0] == '\0' || pass[0] == '\0') {
        console_puts_color("Usage: adduser <username> <password>\n", CON_COLOR_YELLOW);
        return;
    }

    int ret = user_add(name, pass, false);
    if (ret == 0) {
        console_puts_color("User '", CON_COLOR_GREEN);
        console_puts_color(name, CON_COLOR_GREEN);
        console_puts_color("' created.\n", CON_COLOR_GREEN);
    } else if (ret == -2) {
        console_puts_color("User already exists.\n", CON_COLOR_RED);
    } else {
        console_puts_color("Failed to create user (table full).\n", CON_COLOR_RED);
    }
}

