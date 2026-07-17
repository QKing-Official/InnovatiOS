#include <shell.h>
#include <programs.h>
#include <kernel/drivers/console.h>

/* ----------------------------------------------------------------------
 * InnovatiOS shell v4
 *
 * On top of v3 (settings toggles, env vars + $expansion, aliases, ';'
 * chaining, "!!" repeat, history, typo suggestions, numbered prompt):
 *
 *   - Unambiguous prefix shortcuts: typing "wh" runs "whoami" if it's
 *     the only command starting with "wh". If more than one command
 *     matches, you get a list of candidates instead of guessing.
 *   - Multi-command macros: an alias's value can itself contain ';',
 *     so "alias fresh=clear;whoami" makes "fresh" run both commands.
 *     Expansion is capped at 4 levels deep so a self-referential alias
 *     can't loop forever.
 *   - `theme <red|green|cyan|white|reset>` recolors your username in
 *     the prompt, overriding the admin/user default.
 *   - Built-in read-only variables: $USER (your username), $ADMIN
 *     ("1"/"0"), $CMDNUM (current command counter) - usable anywhere
 *     $expansion works, alongside your own `export`ed variables.
 *   - `reboot` now asks "Continue? (yes/no)" before actually calling
 *     prog_reboot(), so a stray Enter can't take the system down.
 *   - `sysinfo` runs uname/uptime/meminfo together (whichever of those
 *     are compiled in).
 *   - `banner` reprints the welcome banner + a tip; `version` prints
 *     the shell version string.
 *   - `help <command>` now shows detail for just that command;
 *     `help` alone still lists everything.
 *   - `history clear` wipes the session history.
 *
 * Still no dependency on anything beyond `console_*` and the
 * `prog_*`/`prog_skip_spaces` functions you already had - all string
 * handling is hand-rolled (sh_* helpers) below.
 * ------------------------------------------------------------------- */

#define CMD_BUF_MAX        256
#define SHELL_HISTORY_SIZE 16
#define NAME_MAX           16
#define VALUE_MAX          64
#define MAX_ALIASES        8
#define MAX_VARS           8
#define ALIAS_MAX_DEPTH    4
#define SHELL_VERSION      "InnovatiOS Shell v4.0"

#ifndef CONFIG_SHELL_HISTORY
#define CONFIG_SHELL_HISTORY 1
#endif
#ifndef CONFIG_SHELL_ECHO
#define CONFIG_SHELL_ECHO 1
#endif
#ifndef CONFIG_SHELL_COLORTEST
#define CONFIG_SHELL_COLORTEST 1
#endif
#ifndef CONFIG_SHELL_SUGGEST
#define CONFIG_SHELL_SUGGEST 1
#endif
#ifndef CONFIG_SHELL_SETTINGS
#define CONFIG_SHELL_SETTINGS 1
#endif
#ifndef CONFIG_SHELL_ALIAS
#define CONFIG_SHELL_ALIAS 1
#endif
#ifndef CONFIG_SHELL_ENV
#define CONFIG_SHELL_ENV 1
#endif
#ifndef CONFIG_SHELL_PREFIX_MATCH
#define CONFIG_SHELL_PREFIX_MATCH 1
#endif
#ifndef CONFIG_SHELL_THEME
#define CONFIG_SHELL_THEME 1
#endif
#ifndef CONFIG_SHELL_CONFIRM_DESTRUCTIVE
#define CONFIG_SHELL_CONFIRM_DESTRUCTIVE 1
#endif
#ifndef CONFIG_SHELL_SYSINFO
#define CONFIG_SHELL_SYSINFO 1
#endif

/* ---------------------------- local state ---------------------------- */

static int g_shell_exit = 0;
static unsigned int g_cmd_number = 1;

#if CONFIG_SHELL_HISTORY
static char history_buf[SHELL_HISTORY_SIZE][CMD_BUF_MAX];
static unsigned int history_count = 0;
#endif

#if CONFIG_SHELL_SETTINGS
typedef struct {
    int show_counter;
    int show_suggestions;
} shell_settings_t;
static shell_settings_t g_settings = { 1, 1 };
#endif

#if CONFIG_SHELL_ALIAS
typedef struct {
    char name[NAME_MAX];
    char value[VALUE_MAX];
    int used;
} kv_t;
static kv_t g_aliases[MAX_ALIASES];
#endif

#if CONFIG_SHELL_ENV
typedef struct {
    char name[NAME_MAX];
    char value[VALUE_MAX];
    int used;
} envvar_t;
static envvar_t g_vars[MAX_VARS];
#endif

#if CONFIG_SHELL_THEME
static int g_theme_override = -1; /* -1 = auto (role-based default) */
#endif

static unsigned int g_motd_index = 0;
static const char *motd_tips[] = {
    "Tip: type 'help' to see all available commands.",
    "Tip: use '!!' to instantly repeat your last command.",
    "Tip: 'history' shows everything you've typed this session.",
    "Tip: only admins can run privileged commands like 'reboot'.",
    "Tip: 'set counter off' hides the [n] counter in your prompt.",
    "Tip: chain commands with ';', e.g. 'clear; whoami'.",
    "Tip: unique prefixes work too - 'wh' can run 'whoami'.",
    "Tip: 'theme cyan' recolors your prompt.",
    "Tip: aliases can hold multiple commands: alias fresh=clear;whoami",
};
#define MOTD_TIP_COUNT (sizeof(motd_tips) / sizeof(motd_tips[0]))

/* ------------------------ tiny local string helpers ------------------- */

static unsigned int sh_strlen(const char *s) {
    unsigned int n = 0;
    while (s[n]) n++;
    return n;
}

static void sh_copy(char *dst, const char *src, unsigned int max) {
    unsigned int i = 0;
    while (src[i] && i + 1 < max) { dst[i] = src[i]; i++; }
    dst[i] = '\0';
}

static int sh_streq(const char *a, const char *b) {
    unsigned int i = 0;
    while (a[i] && b[i]) {
        if (a[i] != b[i]) return 0;
        i++;
    }
    return a[i] == b[i];
}

/* Compares a non-null-terminated `name` of length `len` against a
 * null-terminated literal. */
static int name_eq(const char *name, unsigned int len, const char *lit) {
    if (sh_strlen(lit) != len) return 0;
    for (unsigned int i = 0; i < len; i++) {
        if (name[i] != lit[i]) return 0;
    }
    return 1;
}

/* Copies the first whitespace-delimited word of `cmd` into `out`. */
static void sh_first_word(const char *cmd, char *out, unsigned int max) {
    unsigned int i = 0;
    while (cmd[i] && cmd[i] != ' ' && i + 1 < max) { out[i] = cmd[i]; i++; }
    out[i] = '\0';
}

/* Splits "a b" into two whitespace-separated words. Returns 1 if `a`
 * (the first word) was non-empty. */
static int split_two_words(const char *args, char *a, unsigned int amax,
                            char *b, unsigned int bmax) {
    unsigned int i = 0;
    while (args[i] == ' ') i++;
    unsigned int ai = 0;
    while (args[i] && args[i] != ' ' && ai + 1 < amax) { a[ai++] = args[i++]; }
    a[ai] = '\0';
    while (args[i] == ' ') i++;
    unsigned int bi = 0;
    while (args[i] && bi + 1 < bmax) { b[bi++] = args[i++]; }
    b[bi] = '\0';
    return ai > 0;
}

/* Splits "NAME=value" on the first '='. Returns 1 on success. */
static int split_kv(const char *args, char *name, unsigned int nmax,
                     char *value, unsigned int vmax) {
    unsigned int i = 0;
    while (args[i] && args[i] != '=' && i + 1 < nmax) { name[i] = args[i]; i++; }
    name[i] = '\0';
    if (args[i] != '=') return 0;
    i++;
    unsigned int j = 0;
    while (args[i] && j + 1 < vmax) { value[j++] = args[i++]; }
    value[j] = '\0';
    return 1;
}

static void sh_uint_to_str(unsigned int n, char *buf, unsigned int max) {
    char tmp[12];
    int i = 11;
    tmp[i--] = '\0';
    if (n == 0) {
        tmp[i--] = '0';
    } else {
        while (n > 0 && i >= 0) {
            tmp[i--] = (char)('0' + (n % 10));
            n /= 10;
        }
    }
    sh_copy(buf, &tmp[i + 1], max);
}

static void sh_print_uint(unsigned int n) {
    char buf[12];
    sh_uint_to_str(n, buf, sizeof(buf));
    console_puts(buf);
}

/* O(n) space Levenshtein distance, capped at 15-char words. */
static int sh_edit_distance(const char *a, const char *b) {
    unsigned int la = sh_strlen(a);
    unsigned int lb = sh_strlen(b);
    if (la >= 16 || lb >= 16) return 99;

    int prev[16], curr[16];
    for (unsigned int j = 0; j <= lb; j++) prev[j] = (int)j;

    for (unsigned int i = 1; i <= la; i++) {
        curr[0] = (int)i;
        for (unsigned int j = 1; j <= lb; j++) {
            int cost = (a[i - 1] == b[j - 1]) ? 0 : 1;
            int del = prev[j] + 1;
            int ins = curr[j - 1] + 1;
            int sub = prev[j - 1] + cost;
            int m = del < ins ? del : ins;
            curr[j] = m < sub ? m : sub;
        }
        for (unsigned int j = 0; j <= lb; j++) prev[j] = curr[j];
    }
    return prev[lb];
}

/* Checks whether `cmd` starts with `name` followed by end-of-string or
 * a space (exact command match, not a prefix shortcut). On match,
 * `*args_out` points at the trimmed remainder. */
static int cmd_match(const char *cmd, const char *name, const char **args_out) {
    unsigned int len = sh_strlen(name);
    for (unsigned int i = 0; i < len; i++) {
        if (cmd[i] != name[i]) return 0;
    }
    if (cmd[len] != '\0' && cmd[len] != ' ') return 0;

    const char *rest = cmd + len;
    while (*rest == ' ') rest++;
    if (args_out) *args_out = rest;
    return 1;
}

#if CONFIG_SHELL_HISTORY
static void history_add(const char *cmd) {
    sh_copy(history_buf[history_count % SHELL_HISTORY_SIZE], cmd, CMD_BUF_MAX);
    history_count++;
}
#endif

#if CONFIG_SHELL_ENV
/* Looks up a variable by name+length: checks built-in read-only vars
 * first, then the user-defined table. */
static const char *var_lookup(const char *name, unsigned int len, user_t *user) {
    static char cmdnum_buf[12];

    if (name_eq(name, len, "USER")) return user->username;
    if (name_eq(name, len, "ADMIN")) return user->is_admin ? "1" : "0";
    if (name_eq(name, len, "CMDNUM")) {
        sh_uint_to_str(g_cmd_number, cmdnum_buf, sizeof(cmdnum_buf));
        return cmdnum_buf;
    }

    for (int i = 0; i < MAX_VARS; i++) {
        if (!g_vars[i].used) continue;
        if (sh_strlen(g_vars[i].name) != len) continue;
        unsigned int j = 0;
        while (j < len && g_vars[i].name[j] == name[j]) j++;
        if (j == len) return g_vars[i].value;
    }
    return 0;
}

/* Replaces every $NAME in `in` with its variable value, writing into `out`. */
static void sh_expand_vars(const char *in, char *out, unsigned int max, user_t *user) {
    unsigned int oi = 0;
    unsigned int i = 0;
    while (in[i] && oi + 1 < max) {
        int is_var_start = (in[i] == '$') &&
                            (in[i + 1] == '_' ||
                             (in[i + 1] >= 'A' && in[i + 1] <= 'Z') ||
                             (in[i + 1] >= 'a' && in[i + 1] <= 'z'));
        if (is_var_start) {
            unsigned int start = i + 1;
            unsigned int len = 0;
            while (in[start + len] == '_' ||
                   (in[start + len] >= 'A' && in[start + len] <= 'Z') ||
                   (in[start + len] >= 'a' && in[start + len] <= 'z') ||
                   (in[start + len] >= '0' && in[start + len] <= '9')) {
                len++;
            }
            const char *val = var_lookup(in + start, len, user);
            if (val) {
                unsigned int vi = 0;
                while (val[vi] && oi + 1 < max) out[oi++] = val[vi++];
            }
            i = start + len;
        } else {
            out[oi++] = in[i++];
        }
    }
    out[oi] = '\0';
}
#endif

/* ----------------------------- commands -------------------------------- */

static void cmd_clear(user_t *user, const char *args) {
    (void)user; (void)args;
    console_clear();
}

static void cmd_logout(user_t *user, const char *args) {
    (void)user; (void)args;
    console_puts("Logging out...\n");
    g_shell_exit = 1;
}

static void cmd_version(user_t *user, const char *args) {
    (void)user; (void)args;
    console_puts_color(SHELL_VERSION, CON_COLOR_CYAN);
    console_puts("\n");
}

static void print_banner(void) {
    console_puts("\n");
    console_puts_color("+--------------------------------------+\n", CON_COLOR_CYAN);
    console_puts_color("|           InnovatiOS Shell            |\n", CON_COLOR_CYAN);
    console_puts_color("+--------------------------------------+\n", CON_COLOR_CYAN);
}

static void cmd_banner(user_t *user, const char *args) {
    (void)user; (void)args;
    print_banner();
    console_puts_color(motd_tips[g_motd_index % MOTD_TIP_COUNT], CON_COLOR_WHITE);
    console_puts("\n");
    g_motd_index++;
}

#if CONFIG_SHELL_ECHO
static void cmd_echo(user_t *user, const char *args) {
    (void)user;
    console_puts(args);
    console_puts("\n");
}
#endif

#if CONFIG_SHELL_COLORTEST
static void cmd_colors(user_t *user, const char *args) {
    (void)user; (void)args;
    console_puts("Available console colors used by this shell:\n");
    console_puts_color("  RED\n", CON_COLOR_RED);
    console_puts_color("  GREEN\n", CON_COLOR_GREEN);
    console_puts_color("  CYAN\n", CON_COLOR_CYAN);
    console_puts_color("  WHITE\n", CON_COLOR_WHITE);
}
#endif

#if CONFIG_SHELL_HISTORY
static void cmd_history(user_t *user, const char *args) {
    (void)user;
    if (sh_streq(args, "clear")) {
        history_count = 0;
        console_puts("History cleared.\n");
        return;
    }
    if (history_count == 0) {
        console_puts("No commands in history yet.\n");
        return;
    }
    unsigned int start = (history_count > SHELL_HISTORY_SIZE)
                              ? history_count - SHELL_HISTORY_SIZE
                              : 0;
    for (unsigned int i = start; i < history_count; i++) {
        console_puts_color("  ", CON_COLOR_WHITE);
        sh_print_uint(i + 1);
        console_puts("  ");
        console_puts(history_buf[i % SHELL_HISTORY_SIZE]);
        console_puts("\n");
    }
}
#endif

#if CONFIG_SHELL_SETTINGS
static void cmd_set(user_t *user, const char *args) {
    (void)user;
    if (*args == '\0') {
        console_puts("counter     : ");
        console_puts(g_settings.show_counter ? "on\n" : "off\n");
        console_puts("suggestions : ");
        console_puts(g_settings.show_suggestions ? "on\n" : "off\n");
        console_puts("\nUsage: set <counter|suggestions> <on|off>\n");
        return;
    }

    char key[NAME_MAX], val[VALUE_MAX];
    split_two_words(args, key, sizeof(key), val, sizeof(val));

    int want_on;
    if (sh_streq(val, "on")) {
        want_on = 1;
    } else if (sh_streq(val, "off")) {
        want_on = 0;
    } else {
        console_puts_color("Usage: set <counter|suggestions> <on|off>\n", CON_COLOR_RED);
        return;
    }

    if (sh_streq(key, "counter")) {
        g_settings.show_counter = want_on;
    } else if (sh_streq(key, "suggestions")) {
        g_settings.show_suggestions = want_on;
    } else {
        console_puts_color("Unknown setting: ", CON_COLOR_RED);
        console_puts(key);
        console_puts("\n");
        return;
    }
    console_puts("OK.\n");
}
#endif

#if CONFIG_SHELL_THEME
static void cmd_theme(user_t *user, const char *args) {
    (void)user;
    if (*args == '\0' || sh_streq(args, "status")) {
        console_puts("Current prompt theme: ");
        console_puts(g_theme_override == -1 ? "auto (role-based)\n" : "custom\n");
        console_puts("Usage: theme <red|green|cyan|white|reset>\n");
        return;
    }
    if (sh_streq(args, "red")) g_theme_override = CON_COLOR_RED;
    else if (sh_streq(args, "green")) g_theme_override = CON_COLOR_GREEN;
    else if (sh_streq(args, "cyan")) g_theme_override = CON_COLOR_CYAN;
    else if (sh_streq(args, "white")) g_theme_override = CON_COLOR_WHITE;
    else if (sh_streq(args, "reset")) g_theme_override = -1;
    else {
        console_puts_color("Unknown theme color.\n", CON_COLOR_RED);
        console_puts("Usage: theme <red|green|cyan|white|reset>\n");
        return;
    }
    console_puts("OK.\n");
}
#endif

#if CONFIG_SHELL_SYSINFO
static void cmd_sysinfo(user_t *user, const char *args) {
    (void)args;
    console_puts_color("=== System Information ===\n", CON_COLOR_CYAN);
#if CONFIG_PROG_UNAME
    prog_uname(user, "");
#endif
#if CONFIG_PROG_UPTIME
    prog_uptime(user, "");
#endif
#if CONFIG_PROG_MEMINFO
    prog_meminfo(user, "");
#endif
#if !CONFIG_PROG_UNAME && !CONFIG_PROG_UPTIME && !CONFIG_PROG_MEMINFO
    (void)user;
    console_puts("(no system-info programs enabled in this build)\n");
#endif
}
#endif

#if CONFIG_PROG_REBOOT && CONFIG_SHELL_CONFIRM_DESTRUCTIVE
static void cmd_reboot_confirm(user_t *user, const char *args) {
    console_puts_color("This will reboot the system. Continue? (yes/no) ", CON_COLOR_RED);
    char confirm[8];
    console_readline(confirm, sizeof(confirm), 0);
    const char *c = prog_skip_spaces(confirm);
    if (sh_streq(c, "yes") || sh_streq(c, "y")) {
        prog_reboot(user, args);
    } else {
        console_puts("Reboot cancelled.\n");
    }
}
#endif

#if CONFIG_SHELL_ENV
static void cmd_export(user_t *user, const char *args) {
    (void)user;
    char name[NAME_MAX], value[VALUE_MAX];
    if (!split_kv(args, name, sizeof(name), value, sizeof(value)) || name[0] == '\0') {
        console_puts_color("Usage: export NAME=value\n", CON_COLOR_RED);
        return;
    }
    for (int i = 0; i < MAX_VARS; i++) {
        if (g_vars[i].used && sh_streq(g_vars[i].name, name)) {
            sh_copy(g_vars[i].value, value, VALUE_MAX);
            return;
        }
    }
    for (int i = 0; i < MAX_VARS; i++) {
        if (!g_vars[i].used) {
            g_vars[i].used = 1;
            sh_copy(g_vars[i].name, name, NAME_MAX);
            sh_copy(g_vars[i].value, value, VALUE_MAX);
            return;
        }
    }
    console_puts_color("Variable table full.\n", CON_COLOR_RED);
}

static void cmd_unset(user_t *user, const char *args) {
    (void)user;
    for (int i = 0; i < MAX_VARS; i++) {
        if (g_vars[i].used && sh_streq(g_vars[i].name, args)) {
            g_vars[i].used = 0;
            console_puts("OK.\n");
            return;
        }
    }
    console_puts_color("No such variable.\n", CON_COLOR_RED);
}

static void cmd_env(user_t *user, const char *args) {
    (void)user; (void)args;
    int any = 0;
    console_puts_color("USER", CON_COLOR_GREEN);
    console_puts("=");
    console_puts(user->username);
    console_puts("  (built-in)\n");
    console_puts_color("ADMIN", CON_COLOR_GREEN);
    console_puts("=");
    console_puts(user->is_admin ? "1" : "0");
    console_puts("  (built-in)\n");
    for (int i = 0; i < MAX_VARS; i++) {
        if (g_vars[i].used) {
            any = 1;
            console_puts_color(g_vars[i].name, CON_COLOR_GREEN);
            console_puts("=");
            console_puts(g_vars[i].value);
            console_puts("\n");
        }
    }
    if (!any) console_puts("(no custom variables set - try 'export NAME=value')\n");
}
#endif

#if CONFIG_SHELL_ALIAS
static void cmd_alias(user_t *user, const char *args) {
    (void)user;
    if (*args == '\0') {
        int any = 0;
        for (int i = 0; i < MAX_ALIASES; i++) {
            if (g_aliases[i].used) {
                any = 1;
                console_puts_color(g_aliases[i].name, CON_COLOR_GREEN);
                console_puts(" = ");
                console_puts(g_aliases[i].value);
                console_puts("\n");
            }
        }
        if (!any) console_puts("No aliases set.\n");
        return;
    }
    char name[NAME_MAX], value[VALUE_MAX];
    if (!split_kv(args, name, sizeof(name), value, sizeof(value)) || name[0] == '\0') {
        console_puts_color("Usage: alias name=command\n", CON_COLOR_RED);
        return;
    }
    for (int i = 0; i < MAX_ALIASES; i++) {
        if (g_aliases[i].used && sh_streq(g_aliases[i].name, name)) {
            sh_copy(g_aliases[i].value, value, VALUE_MAX);
            return;
        }
    }
    for (int i = 0; i < MAX_ALIASES; i++) {
        if (!g_aliases[i].used) {
            g_aliases[i].used = 1;
            sh_copy(g_aliases[i].name, name, NAME_MAX);
            sh_copy(g_aliases[i].value, value, VALUE_MAX);
            return;
        }
    }
    console_puts_color("Alias table full.\n", CON_COLOR_RED);
}

static void cmd_unalias(user_t *user, const char *args) {
    (void)user;
    for (int i = 0; i < MAX_ALIASES; i++) {
        if (g_aliases[i].used && sh_streq(g_aliases[i].name, args)) {
            g_aliases[i].used = 0;
            console_puts("OK.\n");
            return;
        }
    }
    console_puts_color("No such alias.\n", CON_COLOR_RED);
}
#endif

static void cmd_help(user_t *user, const char *args);

/* --------------------------- command table ----------------------------- */

typedef void (*shell_handler_t)(user_t *user, const char *args);

typedef struct {
    const char *name;
    shell_handler_t handler;
    const char *desc;
    int admin_only;
} shell_cmd_t;

static const shell_cmd_t commands[] = {
#if CONFIG_PROG_HELP
    { "help",     cmd_help,     "Show command list, or 'help <cmd>' for detail", 0 },
#endif
#if CONFIG_PROG_WHOAMI
    { "whoami",   prog_whoami,  "Show current user info",                  0 },
#endif
#if CONFIG_PROG_USERS
    { "users",    prog_users,   "List all users on the system",            0 },
#endif
#if CONFIG_PROG_ADDUSER
    { "adduser",  prog_adduser, "Add a new user",                          1 },
#endif
    { "clear",    cmd_clear,    "Clear the screen",                        0 },
#if CONFIG_PROG_UNAME
    { "uname",    prog_uname,   "Show system/kernel info",                 0 },
#endif
#if CONFIG_PROG_UPTIME
    { "uptime",   prog_uptime,  "Show system uptime",                      0 },
#endif
#if CONFIG_PROG_MEMINFO
    { "meminfo",  prog_meminfo, "Show memory usage info",                  0 },
#endif
#if CONFIG_PROG_REBOOT
#if CONFIG_SHELL_CONFIRM_DESTRUCTIVE
    { "reboot",   cmd_reboot_confirm, "Reboot the system (asks to confirm)", 1 },
#else
    { "reboot",   prog_reboot,  "Reboot the system",                       1 },
#endif
#endif
#if CONFIG_USER_ADDED_PROGRAMS
    { "matrix",   prog_matrix,  "Run the matrix screensaver",              0 },
#endif
#if CONFIG_SHELL_HISTORY
    { "history",  cmd_history,  "Show history, or 'history clear' to wipe it", 0 },
#endif
#if CONFIG_SHELL_ECHO
    { "echo",     cmd_echo,     "Print text back to the console",          0 },
#endif
#if CONFIG_SHELL_COLORTEST
    { "colors",   cmd_colors,   "Show the console colors this shell uses", 0 },
#endif
#if CONFIG_SHELL_SETTINGS
    { "set",      cmd_set,      "View/change shell settings",              0 },
#endif
#if CONFIG_SHELL_ENV
    { "export",   cmd_export,   "Set an environment variable",             0 },
    { "unset",    cmd_unset,    "Remove an environment variable",          0 },
    { "env",      cmd_env,      "List environment variables",              0 },
#endif
#if CONFIG_SHELL_ALIAS
    { "alias",    cmd_alias,    "Create or list command aliases/macros",   0 },
    { "unalias",  cmd_unalias,  "Remove a command alias",                  0 },
#endif
#if CONFIG_SHELL_THEME
    { "theme",    cmd_theme,    "Recolor your prompt",                     0 },
#endif
#if CONFIG_SHELL_SYSINFO
    { "sysinfo",  cmd_sysinfo,  "Show uname/uptime/meminfo together",      0 },
#endif
    { "banner",   cmd_banner,   "Reprint the welcome banner",              0 },
    { "version",  cmd_version,  "Show shell version",                      0 },
    { "logout",   cmd_logout,   "Log out of the current session",         0 },
};
#define NUM_COMMANDS (sizeof(commands) / sizeof(commands[0]))

static void cmd_help(user_t *user, const char *args) {
    if (*args != '\0') {
        for (unsigned int i = 0; i < NUM_COMMANDS; i++) {
            if (!sh_streq(commands[i].name, args)) continue;
            if (commands[i].admin_only && !user->is_admin) {
                console_puts_color("Permission denied.\n", CON_COLOR_RED);
                return;
            }
            console_puts_color(commands[i].name, CON_COLOR_GREEN);
            console_puts(" - ");
            console_puts(commands[i].desc);
            console_puts("\n");
            if (commands[i].admin_only) {
                console_puts_color("(admin only)\n", CON_COLOR_RED);
            }
            return;
        }
        console_puts_color("No such command: ", CON_COLOR_RED);
        console_puts(args);
        console_puts("\n");
        return;
    }

    console_puts_color("Available commands:\n", CON_COLOR_CYAN);
    for (unsigned int i = 0; i < NUM_COMMANDS; i++) {
        if (commands[i].admin_only && !user->is_admin) continue;

        console_puts("  ");
        console_puts_color(commands[i].name,
                            commands[i].admin_only ? CON_COLOR_RED : CON_COLOR_GREEN);

        unsigned int len = sh_strlen(commands[i].name);
        for (unsigned int p = len; p < 10; p++) console_puts(" ");

        console_puts(" - ");
        console_puts(commands[i].desc);
        console_puts("\n");
    }
    console_puts_color("\nTip: ", CON_COLOR_WHITE);
    console_puts("'help <command>' for detail, ';' chains commands, '!!' repeats\n");
    console_puts("     the last one, and unique prefixes work as shortcuts.\n");
}

/* Runs a single trimmed command segment (no top-level ';' in it,
 * though an alias expansion may reintroduce one - handled below).
 * `depth` guards against a self-referential alias looping forever. */
static void process_segment(user_t *user, const char *raw_seg, int depth) {
    if (depth > ALIAS_MAX_DEPTH) {
        console_puts_color("Alias expands into itself too deeply, aborting.\n", CON_COLOR_RED);
        return;
    }

    const char *seg = raw_seg;

#if CONFIG_SHELL_ALIAS
    char expanded_alias[CMD_BUF_MAX];
    {
        char word[NAME_MAX];
        sh_first_word(seg, word, sizeof(word));
        for (int i = 0; i < MAX_ALIASES; i++) {
            if (g_aliases[i].used && sh_streq(g_aliases[i].name, word)) {
                unsigned int wlen = sh_strlen(word);
                const char *rest = seg + wlen;
                while (*rest == ' ') rest++;

                unsigned int p = 0;
                const char *v = g_aliases[i].value;
                while (*v && p + 1 < sizeof(expanded_alias)) expanded_alias[p++] = *v++;
                if (*rest && p + 1 < sizeof(expanded_alias)) expanded_alias[p++] = ' ';
                while (*rest && p + 1 < sizeof(expanded_alias)) expanded_alias[p++] = *rest++;
                expanded_alias[p] = '\0';

                seg = expanded_alias;
                break;
            }
        }
    }

    /* An alias value containing ';' acts like a tiny multi-command
     * macro - split it and recurse, one level deeper. */
    {
        int has_semi = 0;
        for (unsigned int k = 0; seg[k]; k++) {
            if (seg[k] == ';') { has_semi = 1; break; }
        }
        if (has_semi) {
            char subbuf[CMD_BUF_MAX];
            unsigned int start = 0, i = 0;
            while (1) {
                if (seg[i] == ';' || seg[i] == '\0') {
                    unsigned int len = i - start;
                    if (len >= sizeof(subbuf)) len = sizeof(subbuf) - 1;
                    for (unsigned int k = 0; k < len; k++) subbuf[k] = seg[start + k];
                    subbuf[len] = '\0';

                    const char *trimmed = prog_skip_spaces(subbuf);
                    if (*trimmed != '\0') {
                        process_segment(user, trimmed, depth + 1);
                        if (g_shell_exit) return;
                    }
                    if (seg[i] == '\0') break;
                    start = i + 1;
                }
                i++;
            }
            return;
        }
    }
#endif

    g_cmd_number++;

    const shell_cmd_t *matched = 0;
    const char *args = "";
    for (unsigned int i = 0; i < NUM_COMMANDS; i++) {
        const char *a;
        if (cmd_match(seg, commands[i].name, &a)) {
            matched = &commands[i];
            args = a;
            break;
        }
    }

#if CONFIG_SHELL_PREFIX_MATCH
    if (!matched) {
        char word[16];
        sh_first_word(seg, word, sizeof(word));
        unsigned int wlen = sh_strlen(word);

        if (wlen > 0) {
            const shell_cmd_t *candidates[NUM_COMMANDS];
            unsigned int ncand = 0;
            for (unsigned int i = 0; i < NUM_COMMANDS; i++) {
                unsigned int nlen = sh_strlen(commands[i].name);
                if (nlen <= wlen) continue;
                unsigned int j = 0;
                while (j < wlen && commands[i].name[j] == word[j]) j++;
                if (j == wlen) candidates[ncand++] = &commands[i];
            }
            if (ncand == 1) {
                matched = candidates[0];
                const char *rest = seg + wlen;
                while (*rest == ' ') rest++;
                args = rest;
            } else if (ncand > 1) {
                console_puts_color("Ambiguous command: ", CON_COLOR_RED);
                console_puts(word);
                console_puts(" could mean: ");
                for (unsigned int k = 0; k < ncand; k++) {
                    console_puts(candidates[k]->name);
                    if (k + 1 < ncand) console_puts(", ");
                }
                console_puts("\n");
                return;
            }
        }
    }
#endif

    if (matched) {
        if (matched->admin_only && !user->is_admin) {
            console_puts_color("Permission denied: ", CON_COLOR_RED);
            console_puts(matched->name);
            console_puts(" requires admin privileges.\n");
        } else {
            matched->handler(user, args);
        }
        return;
    }

    console_puts_color("Unknown command: ", CON_COLOR_RED);
    console_puts(seg);
    console_puts("\n");

#if CONFIG_SHELL_SUGGEST
#if CONFIG_SHELL_SETTINGS
    if (!g_settings.show_suggestions) return;
#endif
    {
        char word[16];
        sh_first_word(seg, word, sizeof(word));

        const shell_cmd_t *best = 0;
        int best_dist = 99;
        for (unsigned int i = 0; i < NUM_COMMANDS; i++) {
            int d = sh_edit_distance(word, commands[i].name);
            if (d < best_dist) { best_dist = d; best = &commands[i]; }
        }
        if (best && best_dist > 0 && best_dist <= 2) {
            console_puts("Did you mean '");
            console_puts_color(best->name, CON_COLOR_GREEN);
            console_puts("'?\n");
        }
    }
#endif
}

/* ------------------------------ shell_run ------------------------------ */

void shell_run(user_t *user) {
    char line[CMD_BUF_MAX];

    g_shell_exit = 0;
    g_cmd_number = 1;

    print_banner();
    console_puts("Type 'help' for a list of commands.\n");
    console_puts_color(motd_tips[g_motd_index % MOTD_TIP_COUNT], CON_COLOR_WHITE);
    console_puts("\n\n");
    g_motd_index++;

    while (1) {
#if CONFIG_SHELL_SETTINGS
        if (g_settings.show_counter) {
#endif
            console_puts_color("[", CON_COLOR_WHITE);
            sh_print_uint(g_cmd_number);
            console_puts_color("] ", CON_COLOR_WHITE);
#if CONFIG_SHELL_SETTINGS
        }
#endif

#if CONFIG_SHELL_THEME
        int name_color = (g_theme_override != -1)
                              ? g_theme_override
                              : (user->is_admin ? CON_COLOR_RED : CON_COLOR_GREEN);
#else
        int name_color = user->is_admin ? CON_COLOR_RED : CON_COLOR_GREEN;
#endif
        console_puts_color(user->username, name_color);
        console_puts_color("@innovatios", CON_COLOR_WHITE);
        console_puts_color(user->is_admin ? "# " : "$ ", name_color);

        console_set_color(CON_COLOR_WHITE, CON_COLOR_BG);
        console_readline(line, sizeof(line), 0);

        const char *cmd = prog_skip_spaces(line);
        if (*cmd == '\0') continue;

#if CONFIG_SHELL_HISTORY
        /* "!!" repeats the last full entered line, bash-style. */
        if (cmd[0] == '!' && cmd[1] == '!' && cmd[2] == '\0') {
            if (history_count == 0) {
                console_puts_color("No previous command in history.\n", CON_COLOR_RED);
                continue;
            }
            sh_copy(line, history_buf[(history_count - 1) % SHELL_HISTORY_SIZE], CMD_BUF_MAX);
            cmd = prog_skip_spaces(line);
            console_puts_color(cmd, CON_COLOR_WHITE);
            console_puts("\n");
        }
#endif

#if CONFIG_SHELL_ENV
        char expanded[CMD_BUF_MAX];
        sh_expand_vars(cmd, expanded, sizeof(expanded), user);
#else
        char expanded[CMD_BUF_MAX];
        sh_copy(expanded, cmd, sizeof(expanded));
#endif

#if CONFIG_SHELL_HISTORY
        history_add(expanded);
#endif

        /* Split on ';' and run each piece in order. */
        char seg_buf[CMD_BUF_MAX];
        unsigned int start = 0;
        unsigned int i = 0;
        while (1) {
            if (expanded[i] == ';' || expanded[i] == '\0') {
                unsigned int len = i - start;
                if (len >= sizeof(seg_buf)) len = sizeof(seg_buf) - 1;
                for (unsigned int k = 0; k < len; k++) seg_buf[k] = expanded[start + k];
                seg_buf[len] = '\0';

                const char *trimmed = prog_skip_spaces(seg_buf);
                if (*trimmed != '\0') {
                    process_segment(user, trimmed, 0);
                    if (g_shell_exit) return;
                }

                if (expanded[i] == '\0') break;
                start = i + 1;
            }
            i++;
        }
    }
}