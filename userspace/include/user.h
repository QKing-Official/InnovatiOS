#ifndef KERNEL_PROC_USER_H
#define KERNEL_PROC_USER_H

#include <kernel/types.h>

#define USER_NAME_MAX 32
#define USER_MAX      16

typedef struct {
    bool used;
    u32  uid;
    char username[USER_NAME_MAX];
    u64  password_hash;
    bool is_admin;
} user_t;

void    user_init(void);
user_t *user_authenticate(const char *username, const char *password);
user_t *user_auto_login(void);
int     user_add(const char *username, const char *password, bool is_admin);
user_t *user_get_by_index(u32 index);
u32     user_get_count(void);
user_t *user_get_all(void);
u64     user_hash_password(const char *password);
int     user_change_password(user_t *user, const char *new_password);
int     user_set_admin(const char *username, bool is_admin);

#endif