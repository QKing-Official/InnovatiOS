#include <programs.h>
#include <kernel/drivers/console.h>

#if CONFIG_PIT
#include <kernel/drivers/pit.h>
#endif

void prog_uptime(user_t *user, const char *args) {
    (void)user;
    (void)args;
#if CONFIG_PIT
    u64 ticks = pit_get_ticks();
    u64 seconds = ticks / 100;
    u64 minutes = seconds / 60;
    seconds = seconds % 60;

    char numbuf[21];
    console_puts("Up ");
    console_puts(prog_u64_to_str(minutes, numbuf, sizeof(numbuf)));
    console_puts("m ");
    console_puts(prog_u64_to_str(seconds, numbuf, sizeof(numbuf)));
    console_puts("s (");
    console_puts(prog_u64_to_str(ticks, numbuf, sizeof(numbuf)));
    console_puts(" ticks)\n");
#else
    console_puts("Timer not available.\n");
#endif
}

