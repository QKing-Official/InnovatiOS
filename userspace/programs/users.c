#include <programs.h>
#include <kernel/drivers/console.h>
#include <kernel/lib/string.h>

void prog_users(user_t *user, const char *args) {
    (void)args;
    if (!user->is_admin) {
        console_puts_color("Permission denied.\n", CON_COLOR_RED);
        return;
    }
    char numbuf[21];
    console_puts_color("UID  USERNAME         ROLE\n", CON_COLOR_CYAN);
    console_puts_color("---  ---------------  -----\n", CON_COLOR_GREY);
    for (u32 i = 0; i < USER_MAX; i++) {
        user_t *u = user_get_by_index(i);
        if (!u) continue;

        const char *uid_str = prog_u64_to_str(u->uid, numbuf, sizeof(numbuf));
        console_puts(uid_str);
        for (size_t p = k_strlen(uid_str); p < 5; p++) console_putc(' ');

        console_puts(u->username);
        for (size_t p = k_strlen(u->username); p < 17; p++) console_putc(' ');

        if (u->is_admin)
            console_puts_color("admin\n", CON_COLOR_YELLOW);
        else
            console_puts("user\n");
    }
}

