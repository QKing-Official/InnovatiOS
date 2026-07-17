#include <programs.h>
#include <kernel/drivers/console.h>

#if CONFIG_PMM
#include <kernel/mm/pmm.h>
#endif

void prog_meminfo(user_t *user, const char *args) {
    (void)user;
    (void)args;
#if CONFIG_PMM
    char numbuf[21];
    u64 total = pmm_total_pages();
    u64 free_pg = pmm_free_pages_count();
    u64 used = total - free_pg;

    console_puts_color("Memory Information:\n", CON_COLOR_CYAN);
    console_puts("  Total pages:  ");
    console_puts(prog_u64_to_str(total, numbuf, sizeof(numbuf)));
    console_puts(" (");
    console_puts(prog_u64_to_str((total * 4096) / 1024 / 1024, numbuf, sizeof(numbuf)));
    console_puts(" MB)\n");

    console_puts("  Used pages:   ");
    console_puts_color(prog_u64_to_str(used, numbuf, sizeof(numbuf)), CON_COLOR_YELLOW);
    console_puts("\n");

    console_puts("  Free pages:   ");
    console_puts_color(prog_u64_to_str(free_pg, numbuf, sizeof(numbuf)), CON_COLOR_GREEN);
    console_puts("\n");
#else
    console_puts("PMM not available.\n");
#endif
}

