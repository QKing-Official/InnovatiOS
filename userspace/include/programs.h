#ifndef KERNEL_PROC_PROGRAMS_H
#define KERNEL_PROC_PROGRAMS_H

#include <user.h>

const char *prog_u64_to_str(u64 val, char *buf, size_t bufsz);
const char *prog_skip_spaces(const char *s);
const char *prog_next_word(const char *s, char *word, size_t maxlen);

#if CONFIG_PROG_HELP
void prog_help(user_t *user, const char *args);
#endif

#if CONFIG_PROG_WHOAMI
void prog_whoami(user_t *user, const char *args);
#endif

#if CONFIG_PROG_USERS
void prog_users(user_t *user, const char *args);
#endif

#if CONFIG_PROG_ADDUSER
void prog_adduser(user_t *user, const char *args);
#endif

#if CONFIG_PROG_UNAME
void prog_uname(user_t *user, const char *args);
#endif

#if CONFIG_PROG_UPTIME
void prog_uptime(user_t *user, const char *args);
#endif

#if CONFIG_PROG_MEMINFO
void prog_meminfo(user_t *user, const char *args);
#endif

#if CONFIG_PROG_REBOOT
void prog_reboot(user_t *user, const char *args);
#endif

#include <user_programs.h>

#endif
