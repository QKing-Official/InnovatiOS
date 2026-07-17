#include <programs.h>
#include <kernel/drivers/console.h>

void prog_help(user_t *user, const char *args) {
    (void)user;
    (void)args;
    console_puts_color("Available commands:\n", CON_COLOR_CYAN);
#if CONFIG_PROG_HELP
    console_puts("  help      - Show this help message\n");
#endif
#if CONFIG_PROG_WHOAMI
    console_puts("  whoami    - Print current user\n");
#endif
#if CONFIG_PROG_USERS
    console_puts("  users     - List all users (admin only)\n");
#endif
#if CONFIG_PROG_ADDUSER
    console_puts("  adduser   - Add a user: adduser <name> <pass> (admin only)\n");
#endif
    console_puts("  clear     - Clear the screen\n");
#if CONFIG_PROG_UNAME
    console_puts("  uname     - Print OS information\n");
#endif
#if CONFIG_PROG_UPTIME
    console_puts("  uptime    - Print system uptime\n");
#endif
#if CONFIG_PROG_MEMINFO
    console_puts("  meminfo   - Print memory information\n");
#endif
#if CONFIG_PROG_REBOOT
    console_puts("  reboot    - Reboot the system\n");
#endif
    console_puts("  logout    - Log out\n");
}

