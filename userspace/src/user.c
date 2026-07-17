#include <user.h>
#include <kernel/lib/string.h>

static user_t g_users[USER_MAX];
static u32 g_user_count = 0;
static u32 g_next_uid = 0;

u64 user_hash_password(const char *password) {

    u64 hash = 5381;
    while (*password) {
        hash = ((hash << 5) + hash) + (u8)(*password);
        password++;
    }
    return hash;
}

void user_init(void) {
    k_memset(g_users, 0, sizeof(g_users));
    g_user_count = 0;
    g_next_uid = 0;

    user_add("root",  "innovatios", true);
    user_add("guest", "guest",      false);
}

user_t *user_authenticate(const char *username, const char *password) {
    u64 hash = user_hash_password(password);
    for (u32 i = 0; i < USER_MAX; i++) {
        if (g_users[i].used &&
            k_strcmp(g_users[i].username, username) == 0 &&
            g_users[i].password_hash == hash) {
            return &g_users[i];
        }
    }
    return NULL;
}

int user_add(const char *username, const char *password, bool is_admin) {
    if (g_user_count >= USER_MAX) return -1;

    for (u32 i = 0; i < USER_MAX; i++) {
        if (g_users[i].used && k_strcmp(g_users[i].username, username) == 0) {
            return -2;
        }
    }

    for (u32 i = 0; i < USER_MAX; i++) {
        if (!g_users[i].used) {
            g_users[i].used = true;
            g_users[i].uid = g_next_uid++;
            k_strncpy(g_users[i].username, username, USER_NAME_MAX - 1);
            g_users[i].username[USER_NAME_MAX - 1] = '\0';
            g_users[i].password_hash = user_hash_password(password);
            g_users[i].is_admin = is_admin;
            g_user_count++;
            return 0;
        }
    }
    return -1;
}

user_t *user_get_by_index(u32 index) {
    if (index >= USER_MAX) return NULL;
    if (!g_users[index].used) return NULL;
    return &g_users[index];
}

u32 user_count(void) {
    return g_user_count;
}

