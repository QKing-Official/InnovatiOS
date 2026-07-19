#include <shell.h>
#include <programs.h>
#include <kernel/drivers/console.h>
#if CONFIG_VFS
#include <kernel/fs/vfs.h>
#endif
#include <kernel/lib/string.h>
#include <kernel/mm/heap.h>

#define CMD_BUF_MAX        256
#define SHELL_HISTORY_SIZE 16
#define NAME_MAX           16
#define VALUE_MAX          64
#define MAX_ALIASES        8
#define MAX_VARS           8
#define MAX_THEMES         12
#define ALIAS_MAX_DEPTH    4
#define SHELL_VERSION      "InnovatiOS Shell v5.0"

#ifndef CONFIG_SHELL
#define CONFIG_SHELL 1
#endif
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
#ifndef CONFIG_SHELL_CALC
#define CONFIG_SHELL_CALC 1
#endif
#ifndef CONFIG_SHELL_REPEAT
#define CONFIG_SHELL_REPEAT 1
#endif
#ifndef CONFIG_SHELL_MOTD
#define CONFIG_SHELL_MOTD 1
#endif

#if CONFIG_SHELL

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
    int prompt_style;
} shell_settings_t;
static shell_settings_t g_settings = { 1, 1, 0 };
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
typedef struct {
    char name[NAME_MAX];
    int user_color;
    int at_color;
    int symbol_color;
    char symbol;
    int used;
    int builtin;
} theme_t;
static theme_t g_themes[MAX_THEMES];
static int g_active_theme = -1;
static int g_themes_ready = 0;
#endif

#if CONFIG_SHELL_MOTD
static char g_custom_motd[VALUE_MAX * 2];
static int g_custom_motd_set = 0;
#endif

typedef struct {
    u32 magic;
#if CONFIG_SHELL_SETTINGS
    shell_settings_t settings;
#endif
#if CONFIG_SHELL_ALIAS
    kv_t aliases[MAX_ALIASES];
#endif
#if CONFIG_SHELL_ENV
    envvar_t vars[MAX_VARS];
#endif
#if CONFIG_SHELL_THEME
    theme_t themes[MAX_THEMES];
    int active_theme;
#endif
#if CONFIG_SHELL_MOTD
    char custom_motd[VALUE_MAX * 2];
    int custom_motd_set;
#endif
} shell_state_t;

static void shell_save_state(user_t *user) {
#if CONFIG_VFS
    shell_state_t state;
    k_memset(&state, 0, sizeof(state));
    state.magic = 0x53484C4C;
#if CONFIG_SHELL_SETTINGS
    k_memcpy(&state.settings, &g_settings, sizeof(shell_settings_t));
#endif
#if CONFIG_SHELL_ALIAS
    k_memcpy(state.aliases, g_aliases, sizeof(g_aliases));
#endif
#if CONFIG_SHELL_ENV
    k_memcpy(state.vars, g_vars, sizeof(g_vars));
#endif
#if CONFIG_SHELL_THEME
    k_memcpy(state.themes, g_themes, sizeof(g_themes));
    state.active_theme = g_active_theme;
#endif
#if CONFIG_SHELL_MOTD
    k_strcpy(state.custom_motd, g_custom_motd);
    state.custom_motd_set = g_custom_motd_set;
#endif

    char path[128];
    if (k_strcmp(user->username, "root") == 0) {
        k_strcpy(path, "/root/.shellrc");
    } else {
        k_strcpy(path, "/home/");
        k_strncat(path, user->username, 64);
        k_strncat(path, "/.shellrc", 128);
    }
    
    int fd = vfs_open(path, VFS_MODE_WRITE | VFS_MODE_CREATE, user->uid);
    if (fd >= 0) {
        vfs_write(fd, &state, sizeof(state));
        vfs_close(fd);
    }
#endif
}

static void shell_load_state(user_t *user) {
#if CONFIG_VFS
    char path[128];
    if (k_strcmp(user->username, "root") == 0) {
        k_strcpy(path, "/root/.shellrc");
    } else {
        k_strcpy(path, "/home/");
        k_strncat(path, user->username, 64);
        k_strncat(path, "/.shellrc", 128);
    }
    
    int fd = vfs_open(path, VFS_MODE_READ, user->uid);
    if (fd >= 0) {
        shell_state_t state;
        if (vfs_read(fd, &state, sizeof(state)) == sizeof(state)) {
            if (state.magic == 0x53484C4C) {
#if CONFIG_SHELL_SETTINGS
                k_memcpy(&g_settings, &state.settings, sizeof(shell_settings_t));
#endif
#if CONFIG_SHELL_ALIAS
                k_memcpy(g_aliases, state.aliases, sizeof(g_aliases));
#endif
#if CONFIG_SHELL_ENV
                k_memcpy(g_vars, state.vars, sizeof(g_vars));
#endif
#if CONFIG_SHELL_THEME
                k_memcpy(g_themes, state.themes, sizeof(g_themes));
                g_active_theme = state.active_theme;
#endif
#if CONFIG_SHELL_MOTD
                k_strcpy(g_custom_motd, state.custom_motd);
                g_custom_motd_set = state.custom_motd_set;
#endif
            }
        }
        vfs_close(fd);
    }
#endif
}

static unsigned int g_motd_index = 0;
static const char *motd_tips[] = {
    "Tip: type 'help' to see all available commands.",
    "Tip: use '!!' to instantly repeat your last command.",
    "Tip: 'history' shows everything you've typed this session.",
    "Tip: only admins can run privileged commands like 'reboot'.",
    "Tip: 'set counter off' hides the [n] counter in your prompt.",
    "Tip: chain commands with ';', e.g. 'clear; whoami'.",
    "Tip: unique prefixes work too - 'wh' can run 'whoami'.",
    "Tip: 'theme list' shows every prompt theme you can use.",
    "Tip: aliases can hold multiple commands: alias fresh=clear;whoami",
    "Tip: 'theme save mytheme cyan *' makes your own theme.",
    "Tip: 'history find <term>' searches what you've typed.",
    "Tip: 'repeat 3 whoami' runs a command several times.",
    "Tip: 'calc 6 * 7' does quick arithmetic for you.",
};
#define MOTD_TIP_COUNT (sizeof(motd_tips) / sizeof(motd_tips[0]))

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

static int sh_contains(const char *hay, const char *needle) {
    unsigned int nlen = sh_strlen(needle);
    if (nlen == 0) return 1;
    for (unsigned int i = 0; hay[i]; i++) {
        unsigned int j = 0;
        while (j < nlen && hay[i + j] == needle[j]) j++;
        if (j == nlen) return 1;
    }
    return 0;
}

static int name_eq(const char *name, unsigned int len, const char *lit) {
    if (sh_strlen(lit) != len) return 0;
    for (unsigned int i = 0; i < len; i++) {
        if (name[i] != lit[i]) return 0;
    }
    return 1;
}

static void sh_first_word(const char *cmd, char *out, unsigned int max) {
    unsigned int i = 0;
    while (cmd[i] && cmd[i] != ' ' && i + 1 < max) { out[i] = cmd[i]; i++; }
    out[i] = '\0';
}

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

static void sh_int_to_str(int n, char *buf, unsigned int max) {
    if (n < 0) {
        buf[0] = '-';
        if (max > 1) sh_uint_to_str((unsigned int)(-n), buf + 1, max - 1);
    } else {
        sh_uint_to_str((unsigned int)n, buf, max);
    }
}

static void sh_print_int(int n) {
    char buf[16];
    sh_int_to_str(n, buf, sizeof(buf));
    console_puts(buf);
}

static int sh_atoi(const char *s) {
    int sign = 1;
    unsigned int i = 0;
    if (s[0] == '-') { sign = -1; i = 1; }
    else if (s[0] == '+') { i = 1; }
    int result = 0;
    while (s[i] >= '0' && s[i] <= '9') {
        result = result * 10 + (int)(s[i] - '0');
        i++;
    }
    return result * sign;
}

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

#if CONFIG_SHELL_THEME
static int theme_find(const char *name) {
    for (int i = 0; i < MAX_THEMES; i++) {
        if (g_themes[i].used && sh_streq(g_themes[i].name, name)) return i;
    }
    return -1;
}

static int theme_register(const char *name, int uc, int ac, int sc, char sym, int builtin) {
    int idx = theme_find(name);
    if (idx == -1) {
        for (int i = 0; i < MAX_THEMES; i++) {
            if (!g_themes[i].used) { idx = i; break; }
        }
    }
    if (idx == -1) return -1;
    g_themes[idx].used = 1;
    sh_copy(g_themes[idx].name, name, NAME_MAX);
    g_themes[idx].user_color = uc;
    g_themes[idx].at_color = ac;
    g_themes[idx].symbol_color = sc;
    g_themes[idx].symbol = sym;
    g_themes[idx].builtin = builtin;
    return idx;
}

static void shell_init_themes(void) {
    if (g_themes_ready) return;
    g_themes_ready = 1;
    theme_register("default", -1, CON_COLOR_WHITE, -1, '\0', 1);
    theme_register("red", CON_COLOR_RED, CON_COLOR_WHITE, CON_COLOR_RED, '\0', 1);
    theme_register("green", CON_COLOR_GREEN, CON_COLOR_WHITE, CON_COLOR_GREEN, '\0', 1);
    theme_register("cyan", CON_COLOR_CYAN, CON_COLOR_WHITE, CON_COLOR_CYAN, '\0', 1);
    theme_register("white", CON_COLOR_WHITE, CON_COLOR_WHITE, CON_COLOR_WHITE, '\0', 1);
    theme_register("matrix", CON_COLOR_GREEN, CON_COLOR_GREEN, CON_COLOR_GREEN, '>', 1);
    theme_register("fire", CON_COLOR_RED, CON_COLOR_RED, CON_COLOR_WHITE, '>', 1);
    theme_register("ice", CON_COLOR_CYAN, CON_COLOR_CYAN, CON_COLOR_WHITE, '>', 1);
    theme_register("ghost", CON_COLOR_WHITE, CON_COLOR_WHITE, CON_COLOR_WHITE, '~', 1);
    theme_register("royal", CON_COLOR_CYAN, CON_COLOR_RED, CON_COLOR_WHITE, '#', 1);
    theme_register("candy", CON_COLOR_WHITE, CON_COLOR_RED, CON_COLOR_CYAN, '*', 1);
}

static int color_from_name(const char *s, int *out) {
    if (sh_streq(s, "red")) { *out = CON_COLOR_RED; return 1; }
    if (sh_streq(s, "green")) { *out = CON_COLOR_GREEN; return 1; }
    if (sh_streq(s, "cyan")) { *out = CON_COLOR_CYAN; return 1; }
    if (sh_streq(s, "white")) { *out = CON_COLOR_WHITE; return 1; }
    return 0;
}
#endif

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

static void print_motd(void) {
#if CONFIG_SHELL_MOTD
    if (g_custom_motd_set) {
        console_puts_color(g_custom_motd, CON_COLOR_WHITE);
        console_puts("\n");
        return;
    }
#endif
    console_puts_color(motd_tips[g_motd_index % MOTD_TIP_COUNT], CON_COLOR_WHITE);
    console_puts("\n");
    g_motd_index++;
}

static void cmd_banner(user_t *user, const char *args) {
    (void)user; (void)args;
    print_banner();
    print_motd();
}

#if CONFIG_SHELL_ECHO
static void cmd_echo(user_t *user, const char *args) {
    (void)user;
    int len = sh_strlen(args);
    int redir_pos = -1;
    for (int i = 0; i < len; i++) {
        if (args[i] == '>') {
            redir_pos = i;
            break;
        }
    }

    if (redir_pos != -1) {
        char filename[32];
        int j = redir_pos + 1;
        while (args[j] == ' ') j++;
        int k = 0;
        while (args[j] && k < 31) filename[k++] = args[j++];
        filename[k] = '\0';

#if CONFIG_VFS
        int fd = vfs_open(filename, VFS_MODE_WRITE | VFS_MODE_CREATE, user->uid);
        if (fd >= 0) {
            vfs_write(fd, args, redir_pos);
            vfs_write(fd, "\n", 1);
            vfs_close(fd);
        } else {
            console_puts_color("Failed to open file for writing\n", CON_COLOR_RED);
        }
#else
        console_puts_color("VFS disabled: cannot redirect to file\n", CON_COLOR_RED);
#endif
    } else {
        console_puts(args);
        console_puts("\n");
    }
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

    char sub[NAME_MAX], term[VALUE_MAX];
    split_two_words(args, sub, sizeof(sub), term, sizeof(term));
    int filter = sh_streq(sub, "find");
    if (filter && term[0] == '\0') {
        console_puts_color("Usage: history find <term>\n", CON_COLOR_RED);
        return;
    }

    if (history_count == 0) {
        console_puts("No commands in history yet.\n");
        return;
    }

    unsigned int start = (history_count > SHELL_HISTORY_SIZE)
                              ? history_count - SHELL_HISTORY_SIZE
                              : 0;
    int shown = 0;
    for (unsigned int i = start; i < history_count; i++) {
        const char *line = history_buf[i % SHELL_HISTORY_SIZE];
        if (filter && !sh_contains(line, term)) continue;
        console_puts_color("  ", CON_COLOR_WHITE);
        sh_print_uint(i + 1);
        console_puts("  ");
        console_puts(line);
        console_puts("\n");
        shown = 1;
    }
    if (filter && !shown) console_puts("No matches.\n");
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
        console_puts("prompt      : ");
        console_puts(g_settings.prompt_style == 0 ? "bracket\n" :
                      g_settings.prompt_style == 1 ? "arrow\n" : "minimal\n");
        console_puts("\nUsage: set <counter|suggestions> <on|off>\n");
        console_puts("       set prompt <bracket|arrow|minimal>\n");
        return;
    }

    char key[NAME_MAX], val[VALUE_MAX];
    split_two_words(args, key, sizeof(key), val, sizeof(val));

    if (sh_streq(key, "prompt")) {
        if (sh_streq(val, "bracket")) g_settings.prompt_style = 0;
        else if (sh_streq(val, "arrow")) g_settings.prompt_style = 1;
        else if (sh_streq(val, "minimal")) g_settings.prompt_style = 2;
        else {
            console_puts_color("Usage: set prompt <bracket|arrow|minimal>\n", CON_COLOR_RED);
            return;
        }
        console_puts("OK.\n");
        shell_save_state(user);
        return;
    }

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
    shell_save_state(user);
}
#endif

#if CONFIG_SHELL_THEME
static void cmd_theme(user_t *user, const char *args) {
    (void)user;
    shell_init_themes();

    if (*args == '\0' || sh_streq(args, "status")) {
        console_puts("Active theme: ");
        console_puts_color(g_active_theme == -1 ? "default" : g_themes[g_active_theme].name, CON_COLOR_CYAN);
        console_puts("\n");
        console_puts("Usage: theme <name> | theme list | theme save <name> <color> [symbol]\n");
        console_puts("       theme delete <name> | theme reset\n");
        return;
    }

    char sub[NAME_MAX], rest[VALUE_MAX];
    split_two_words(args, sub, sizeof(sub), rest, sizeof(rest));

    if (sh_streq(sub, "list")) {
        console_puts_color("Available themes:\n", CON_COLOR_CYAN);
        for (int i = 0; i < MAX_THEMES; i++) {
            if (!g_themes[i].used) continue;
            console_puts("  ");
            int show_color = (g_themes[i].symbol_color == -1) ? CON_COLOR_WHITE : g_themes[i].symbol_color;
            console_puts_color(g_themes[i].name, show_color);
            console_puts(g_themes[i].builtin ? "  (builtin)\n" : "  (custom)\n");
        }
        return;
    }

    if (sh_streq(sub, "reset")) {
        g_active_theme = -1;
        console_puts("OK.\n");
        shell_save_state(user);
        return;
    }

    if (sh_streq(sub, "delete")) {
        int idx = theme_find(rest);
        if (idx == -1) {
            console_puts_color("No such theme.\n", CON_COLOR_RED);
            return;
        }
        if (g_themes[idx].builtin) {
            console_puts_color("Cannot delete a builtin theme.\n", CON_COLOR_RED);
            return;
        }
        g_themes[idx].used = 0;
        if (g_active_theme == idx) g_active_theme = -1;
        console_puts("OK.\n");
        shell_save_state(user);
        return;
    }

    if (sh_streq(sub, "save")) {
        char name[NAME_MAX], color_and_sym[VALUE_MAX];
        split_two_words(rest, name, sizeof(name), color_and_sym, sizeof(color_and_sym));
        char color_str[NAME_MAX], symbol_str[NAME_MAX];
        split_two_words(color_and_sym, color_str, sizeof(color_str), symbol_str, sizeof(symbol_str));

        int color;
        if (name[0] == '\0' || !color_from_name(color_str, &color)) {
            console_puts_color("Usage: theme save <name> <red|green|cyan|white> [symbol]\n", CON_COLOR_RED);
            return;
        }
        int existing = theme_find(name);
        if (existing != -1 && g_themes[existing].builtin) {
            console_puts_color("Cannot overwrite a builtin theme name.\n", CON_COLOR_RED);
            return;
        }
        char sym = symbol_str[0] ? symbol_str[0] : '\0';
        int idx = theme_register(name, color, color, color, sym, 0);
        if (idx == -1) {
            console_puts_color("Theme table full.\n", CON_COLOR_RED);
            return;
        }
        console_puts("OK.\n");
        shell_save_state(user);
        return;
    }

    int idx = theme_find(sub);
    if (idx == -1) {
        console_puts_color("Unknown theme: ", CON_COLOR_RED);
        console_puts(sub);
        console_puts("\n");
        return;
    }
    g_active_theme = idx;
    console_puts("OK.\n");
    shell_save_state(user);
}
#endif

#if CONFIG_SHELL_MOTD
static void cmd_motd(user_t *user, const char *args) {
    (void)user;
    if (*args == '\0') {
        print_motd();
        return;
    }

    char sub[NAME_MAX], rest[VALUE_MAX];
    split_two_words(args, sub, sizeof(sub), rest, sizeof(rest));

    if (sh_streq(sub, "reset")) {
        g_custom_motd_set = 0;
        g_custom_motd[0] = '\0';
        console_puts("OK.\n");
        shell_save_state(user);
        return;
    }

    if (sh_streq(sub, "set")) {
        if (rest[0] == '\0') {
            console_puts_color("Usage: motd set <text>\n", CON_COLOR_RED);
            return;
        }
        sh_copy(g_custom_motd, rest, sizeof(g_custom_motd));
        g_custom_motd_set = 1;
        console_puts("OK.\n");
        shell_save_state(user);
        return;
    }

    console_puts_color("Usage: motd | motd set <text> | motd reset\n", CON_COLOR_RED);
}
#endif

#if CONFIG_SHELL_CALC
static void cmd_calc(user_t *user, const char *args) {
    (void)user;
    char a[VALUE_MAX], op[NAME_MAX], b[VALUE_MAX];
    unsigned int i = 0;
    while (args[i] == ' ') i++;
    unsigned int ai = 0;
    while (args[i] && args[i] != ' ' && ai + 1 < sizeof(a)) a[ai++] = args[i++];
    a[ai] = '\0';
    while (args[i] == ' ') i++;
    unsigned int oi = 0;
    while (args[i] && args[i] != ' ' && oi + 1 < sizeof(op)) op[oi++] = args[i++];
    op[oi] = '\0';
    while (args[i] == ' ') i++;
    unsigned int bi = 0;
    while (args[i] && bi + 1 < sizeof(b)) b[bi++] = args[i++];
    b[bi] = '\0';

    if (a[0] == '\0' || op[0] == '\0' || b[0] == '\0') {
        console_puts_color("Usage: calc <num> <+|-|*|/> <num>\n", CON_COLOR_RED);
        return;
    }

    int x = sh_atoi(a);
    int y = sh_atoi(b);
    int r;

    if (sh_streq(op, "+")) r = x + y;
    else if (sh_streq(op, "-")) r = x - y;
    else if (sh_streq(op, "*")) r = x * y;
    else if (sh_streq(op, "/")) {
        if (y == 0) {
            console_puts_color("Division by zero.\n", CON_COLOR_RED);
            return;
        }
        r = x / y;
    } else {
        console_puts_color("Unknown operator. Use + - * /\n", CON_COLOR_RED);
        return;
    }

    sh_print_int(r);
    console_puts("\n");
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
    char name[NAME_MAX], value[VALUE_MAX];
    if (!split_kv(args, name, sizeof(name), value, sizeof(value)) || name[0] == '\0') {
        console_puts_color("Usage: export NAME=value\n", CON_COLOR_RED);
        return;
    }
    for (int i = 0; i < MAX_VARS; i++) {
        if (g_vars[i].used && sh_streq(g_vars[i].name, name)) {
            sh_copy(g_vars[i].value, value, VALUE_MAX);
            console_puts("OK.\n");
            shell_save_state(user);
            return;
        }
    }
    for (int i = 0; i < MAX_VARS; i++) {
        if (!g_vars[i].used) {
            g_vars[i].used = 1;
            sh_copy(g_vars[i].name, name, NAME_MAX);
            sh_copy(g_vars[i].value, value, VALUE_MAX);
            console_puts("OK.\n");
            shell_save_state(user);
            return;
        }
    }
    console_puts_color("Variable table full.\n", CON_COLOR_RED);
}

static void cmd_unset(user_t *user, const char *args) {
    for (int i = 0; i < MAX_VARS; i++) {
        if (g_vars[i].used && sh_streq(g_vars[i].name, args)) {
            g_vars[i].used = 0;
            console_puts("OK.\n");
            shell_save_state(user);
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
            console_puts("OK.\n");
            shell_save_state(user);
            return;
        }
    }
    for (int i = 0; i < MAX_ALIASES; i++) {
        if (!g_aliases[i].used) {
            g_aliases[i].used = 1;
            sh_copy(g_aliases[i].name, name, NAME_MAX);
            sh_copy(g_aliases[i].value, value, VALUE_MAX);
            console_puts("OK.\n");
            shell_save_state(user);
            return;
        }
    }
    console_puts_color("Alias table full.\n", CON_COLOR_RED);
}

static void cmd_unalias(user_t *user, const char *args) {
    for (int i = 0; i < MAX_ALIASES; i++) {
        if (g_aliases[i].used && sh_streq(g_aliases[i].name, args)) {
            g_aliases[i].used = 0;
            console_puts("OK.\n");
            shell_save_state(user);
            return;
        }
    }
    console_puts_color("No such alias.\n", CON_COLOR_RED);
}
#endif

static void cmd_help(user_t *user, const char *args);
static void process_segment(user_t *user, const char *raw_seg, int depth);

#if CONFIG_SHELL_REPEAT
static void cmd_repeat(user_t *user, const char *args) {
    char count_str[NAME_MAX];
    unsigned int i = 0;
    while (args[i] == ' ') i++;
    unsigned int ci = 0;
    while (args[i] && args[i] != ' ' && ci + 1 < sizeof(count_str)) count_str[ci++] = args[i++];
    count_str[ci] = '\0';
    while (args[i] == ' ') i++;
    const char *rest = args + i;

    if (count_str[0] == '\0' || rest[0] == '\0') {
        console_puts_color("Usage: repeat <count> <command>\n", CON_COLOR_RED);
        return;
    }

    int n = sh_atoi(count_str);
    if (n <= 0 || n > 50) {
        console_puts_color("Count must be between 1 and 50.\n", CON_COLOR_RED);
        return;
    }

    for (int k = 0; k < n; k++) {
        process_segment(user, rest, 1);
        if (g_shell_exit) return;
    }
}
#endif

typedef void (*shell_handler_t)(user_t *user, const char *args);

typedef struct {
    const char *name;
    shell_handler_t handler;
    const char *desc;
    int admin_only;
} shell_cmd_t;

#if CONFIG_VFS
static void cmd_mkfs(user_t *user, const char *args) {
    (void)user; (void)args;
    vfs_mkfs();
}

static void cmd_ls(user_t *user, const char *args) {
    (void)user;
    char target_dir[64];
    sh_first_word(args, target_dir, sizeof(target_dir));
    vfs_dirent_t dirent;
    int index = 0;
    while (vfs_readdir(target_dir[0] ? target_dir : NULL, index++, &dirent)) {
        if (dirent.flags & VFS_FLAG_DIRECTORY) {
            console_puts_color(dirent.name, CON_COLOR_CYAN);
            console_puts("/  ");
        } else {
            console_puts(dirent.name);
            console_puts("  ");
            char buf[16];
            sh_uint_to_str(dirent.size, buf, sizeof(buf));
            console_puts(buf);
            console_puts(" bytes  ");
        }
    }
    console_puts("\n");
}

static void cmd_cat(user_t *user, const char *args) {
    (void)user;
    char filename[32];
    sh_first_word(args, filename, sizeof(filename));
    if (!filename[0]) {
        console_puts_color("Usage: cat <filename>\n", CON_COLOR_RED);
        return;
    }
    int fd = vfs_open(filename, VFS_MODE_READ, user->uid);
    if (fd >= 0) {
        char buf[65];
        int bytes;
        while ((bytes = vfs_read(fd, buf, 64)) > 0) {
            buf[bytes] = '\0';
            console_puts(buf);
        }
        vfs_close(fd);
        console_puts("\n");
    } else {
        console_puts_color("File not found\n", CON_COLOR_RED);
    }
}

static void cmd_touch(user_t *user, const char *args) {
    (void)user;
    char filename[32];
    sh_first_word(args, filename, sizeof(filename));
    if (!filename[0]) {
        console_puts_color("Usage: touch <filename>\n", CON_COLOR_RED);
        return;
    }
    int fd = vfs_open(filename, VFS_MODE_WRITE | VFS_MODE_CREATE, user->uid);
    if (fd >= 0) vfs_close(fd);
}

static void cmd_rm(user_t *user, const char *args) {
    (void)user;
    char filename[32];
    sh_first_word(args, filename, sizeof(filename));
    if (!filename[0]) {
        console_puts_color("Usage: rm <filename>\n", CON_COLOR_RED);
        return;
    }
    if (vfs_rm(filename, user->uid) < 0) {
        console_puts_color("File not found\n", CON_COLOR_RED);
    }
}
#endif


static void cmd_cd(user_t *user, const char *args) {
    char path[128];
    sh_first_word(args, path, sizeof(path));
    if (!path[0]) {
        char homedir[64];
        if (k_strcmp(user->username, "root") == 0) {
            k_strcpy(homedir, "/root");
        } else {
            k_strcpy(homedir, "/home/");
            k_strncat(homedir, user->username, 64);
        }
        vfs_set_cwd(homedir);
        return;
    }
    vfs_set_cwd(path);
}

static void cmd_pwd(user_t *user, const char *args) {
    (void)user; (void)args;
    console_puts(vfs_get_cwd());
    console_puts("\n");
}

static void cmd_mkdir(user_t *user, const char *args) {
    char dirname[64];
    sh_first_word(args, dirname, sizeof(dirname));
    if (!dirname[0]) {
        console_puts_color("Usage: mkdir <name>\n", CON_COLOR_RED);
        return;
    }
    if (vfs_mkdir(dirname, user->uid, 0) < 0) {
        console_puts_color("Failed to create directory\n", CON_COLOR_RED);
    }
}

static void cmd_promote(user_t *user, const char *args) {
    if (!user->is_admin) {
        console_puts_color("Access denied.\n", CON_COLOR_RED);
        return;
    }
    char target[32];
    sh_first_word(args, target, sizeof(target));
    if (!target[0]) return;
    if (user_set_admin(target, true) == 0) console_puts("User promoted.\n");
    else console_puts("User not found.\n");
}

static void cmd_downgrade(user_t *user, const char *args) {
    if (!user->is_admin) {
        console_puts_color("Access denied.\n", CON_COLOR_RED);
        return;
    }
    char target[32];
    sh_first_word(args, target, sizeof(target));
    if (!target[0]) return;
    if (user_set_admin(target, false) == 0) console_puts("User downgraded.\n");
    else console_puts("User not found.\n");
}

static void cmd_passwd(user_t *user, const char *args) {
    char newpass[32];
    sh_first_word(args, newpass, sizeof(newpass));
    if (!newpass[0]) {
        console_puts_color("Usage: passwd <new_password>\n", CON_COLOR_RED);
        return;
    }
    user_change_password(user, newpass);
    console_puts("Password updated.\n");
}

static const shell_cmd_t commands[] = {
    { "cd",       cmd_cd,       "Change directory",                      0 },
    { "pwd",      cmd_pwd,      "Print working directory",               0 },
    { "mkdir",    cmd_mkdir,    "Create a directory",                    0 },
    { "promote",  cmd_promote,  "Promote a user to admin",               1 },
    { "downgrade",cmd_downgrade,"Downgrade a user from admin",           1 },
    { "passwd",   cmd_passwd,   "Change your password",                  0 },

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
    USER_PROGRAMS_LIST
#endif
#if CONFIG_VFS
    { "mkfs",     cmd_mkfs,     "Format the persistent disk (InnoFS)",     1 },
    { "ls",       cmd_ls,       "List files in the root directory",        0 },
    { "cat",      cmd_cat,      "Read the contents of a file",             0 },
    { "touch",    cmd_touch,    "Create an empty file",                    0 },
    { "rm",       cmd_rm,       "Delete a file",                           0 },
#endif
#if CONFIG_SHELL_HISTORY
    { "history",  cmd_history,  "Show history, 'find <term>' or 'clear' it", 0 },
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
    { "theme",    cmd_theme,    "Manage prompt themes (try 'theme list')", 0 },
#endif
#if CONFIG_SHELL_MOTD
    { "motd",     cmd_motd,     "Show/set the login tip message",          0 },
#endif
#if CONFIG_SHELL_CALC
    { "calc",     cmd_calc,     "Quick arithmetic: calc <n> <op> <n>",     0 },
#endif
#if CONFIG_SHELL_REPEAT
    { "repeat",   cmd_repeat,   "Run a command several times",             0 },
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

void shell_run(user_t *user) {
    char line[CMD_BUF_MAX];

    g_shell_exit = 0;
    g_cmd_number = 1;

#if CONFIG_VFS
    char homedir[64];
    if (k_strcmp(user->username, "root") == 0) {
        k_strcpy(homedir, "/root");
    } else {
        k_strcpy(homedir, "/home/");
        k_strncat(homedir, user->username, 64);
    }
    vfs_set_cwd(homedir);
#endif

#if CONFIG_SHELL_THEME
    shell_init_themes();
#endif

    shell_load_state(user);

    print_banner();
    console_puts("Type 'help' for a list of commands.\n");
    print_motd();
    console_puts("\n");

    while (1) {
#if CONFIG_SHELL_SETTINGS
        if (g_settings.show_counter) {
            if (g_settings.prompt_style == 1) {
                sh_print_uint(g_cmd_number);
                console_puts_color(" > ", CON_COLOR_WHITE);
            } else {
                console_puts_color("[", CON_COLOR_WHITE);
                sh_print_uint(g_cmd_number);
                console_puts_color("] ", CON_COLOR_WHITE);
            }
        }
#endif

        int name_color, at_color, symbol_color;
        char symbol;
#if CONFIG_SHELL_THEME
        if (g_active_theme == -1) {
            name_color = user->is_admin ? CON_COLOR_RED : CON_COLOR_GREEN;
            at_color = CON_COLOR_WHITE;
            symbol_color = name_color;
            symbol = '\0';
        } else {
            theme_t *t = &g_themes[g_active_theme];
            name_color = (t->user_color == -1)
                             ? (user->is_admin ? CON_COLOR_RED : CON_COLOR_GREEN)
                             : t->user_color;
            at_color = t->at_color;
            symbol_color = (t->symbol_color == -1) ? name_color : t->symbol_color;
            symbol = t->symbol;
        }
#else
        name_color = user->is_admin ? CON_COLOR_RED : CON_COLOR_GREEN;
        at_color = CON_COLOR_WHITE;
        symbol_color = name_color;
        symbol = '\0';
#endif

        console_puts_color(user->username, name_color);
#if CONFIG_SHELL_SETTINGS
        if (g_settings.prompt_style != 2) {
            console_puts_color("@innovatios", at_color);
        }
#else
        console_puts_color("@innovatios", at_color);
#endif

#if CONFIG_VFS
        console_puts_color(" ", CON_COLOR_WHITE);
        console_puts_color(vfs_get_cwd(), CON_COLOR_CYAN);
#endif

        if (symbol) {
            char symstr[2];
            symstr[0] = symbol;
            symstr[1] = '\0';
            console_puts_color(symstr, symbol_color);
            console_puts(" ");
        } else {
            console_puts_color(user->is_admin ? "# " : "$ ", symbol_color);
        }

        console_set_color(CON_COLOR_WHITE, CON_COLOR_BG);
        console_readline(line, sizeof(line), 0);

        const char *cmd = prog_skip_spaces(line);
        if (*cmd == '\0') continue;

#if CONFIG_SHELL_HISTORY
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

#else

void shell_run(user_t *user) {
    (void)user;
    console_puts("Shell is disabled in this build.\n");
}

#endif