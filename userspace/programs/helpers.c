#include <programs.h>
#include <kernel/lib/string.h>

const char *prog_u64_to_str(u64 val, char *buf, size_t bufsz) {
    int i = (int)bufsz - 1;
    buf[i--] = '\0';
    if (val == 0) {
        buf[i--] = '0';
    } else {
        while (val > 0 && i >= 0) {
            buf[i--] = (char)('0' + (val % 10));
            val /= 10;
        }
    }
    return &buf[i + 1];
}

const char *prog_skip_spaces(const char *s) {
    while (*s == ' ') s++;
    return s;
}

const char *prog_next_word(const char *s, char *word, size_t maxlen) {
    s = prog_skip_spaces(s);
    size_t i = 0;
    while (*s && *s != ' ' && i < maxlen - 1) {
        word[i++] = *s++;
    }
    word[i] = '\0';
    return s;
}

