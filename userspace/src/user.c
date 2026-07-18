#include <user.h>
#include <kernel/lib/string.h>
#if CONFIG_VFS
#include <kernel/fs/vfs.h>
#endif

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

#if CONFIG_VFS
static void user_save_to_disk(void) {
    int fd = vfs_open("/root/users.cfg", VFS_MODE_WRITE | VFS_MODE_CREATE, 0); // uid 0
    if (fd >= 0) {
        vfs_write(fd, &g_user_count, sizeof(g_user_count));
        vfs_write(fd, &g_next_uid, sizeof(g_next_uid));
        vfs_write(fd, g_users, sizeof(g_users));
        vfs_close(fd);
    }
}

static bool user_load_from_disk(void) {
    int fd = vfs_open("/root/users.cfg", VFS_MODE_READ, 0); // uid 0
    if (fd >= 0) {
        if (vfs_read(fd, &g_user_count, sizeof(g_user_count)) <= 0) { vfs_close(fd); return false; }
        if (vfs_read(fd, &g_next_uid, sizeof(g_next_uid)) <= 0) { vfs_close(fd); return false; }
        if (vfs_read(fd, g_users, sizeof(g_users)) <= 0) { vfs_close(fd); return false; }
        vfs_close(fd);
        return true;
    }
    return false;
}
#else
static void user_save_to_disk(void) {}
static bool user_load_from_disk(void) { return false; }
#endif

void user_init(void) {
    k_memset(g_users, 0, sizeof(g_users));
    g_user_count = 0;
    g_next_uid = 0;

    if (!user_load_from_disk()) {
        user_add("root",  "innovatios", true);
        user_add("guest", "guest",      false);
    }
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

user_t *user_auto_login(void) {
    for (u32 i = 0; i < USER_MAX; i++) {
        if (g_users[i].used && g_users[i].is_admin) {
            return &g_users[i];
        }
    }
    for (u32 i = 0; i < USER_MAX; i++) {
        if (g_users[i].used) {
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
            user_save_to_disk();
            
#if CONFIG_VFS
            // Create home directory for the new user
            char homedir[64];
            if (k_strcmp(username, "root") == 0) {
                k_strcpy(homedir, "/root");
            } else {
                k_strcpy(homedir, "/home/");
                k_strncat(homedir, username, 64);
                vfs_mkdir(homedir, g_users[i].uid, 0x04); // owner_only
            }

            // Create welcome file
            char welcome_file[128];
            k_strcpy(welcome_file, homedir);
            k_strncat(welcome_file, "/welcome.txt", 128);
            int wfd = vfs_open(welcome_file, VFS_MODE_WRITE | VFS_MODE_CREATE, g_users[i].uid);
            if (wfd >= 0) {
                const char *msg = "Welcome to InnovatiOS!\nEnjoy your new account.\n";
                vfs_write(wfd, msg, k_strlen(msg));
                vfs_close(wfd);
            }
#endif
            
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

u32 user_get_count(void) {
    return g_user_count;
}

user_t *user_get_all(void) {
    return g_users;
}

int user_change_password(user_t *user, const char *new_password) {
    if (!user) return -1;
    user->password_hash = user_hash_password(new_password);
    user_save_to_disk();
    return 0;
}

int user_set_admin(const char *username, bool is_admin) {
    for (u32 i = 0; i < USER_MAX; i++) {
        if (g_users[i].used && k_strcmp(g_users[i].username, username) == 0) {
            g_users[i].is_admin = is_admin;
            user_save_to_disk();
            return 0;
        }
    }
    return -1;
}
