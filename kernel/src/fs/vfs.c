#include <kernel/fs/vfs.h>
#include <kernel/fs/innofs.h>
#include <kernel/lib/string.h>

static char g_cwd[256] = "/";

int vfs_init(void) {
    return innofs_init();
}

int vfs_mkfs(void) {
    return innofs_mkfs();
}

static void resolve_path(const char *name, char *abspath) {
    char temp[256];
    if (name[0] == '/') {
        k_strncpy(temp, name, 256);
    } else {
        k_strncpy(temp, g_cwd, 256);
        int len = k_strlen(temp);
        if (len > 0 && temp[len-1] != '/') {
            k_strncat(temp, "/", 256);
        }
        k_strncat(temp, name, 256);
    }

    char *stack[32];
    int top = 0;
    char *p = temp;

    while (*p) {
        while (*p == '/') p++;
        if (!*p) break;

        char *start = p;
        while (*p && *p != '/') p++;
        if (*p) {
            *p = '\0';
            p++;
        }

        if (k_strcmp(start, ".") == 0) {
            continue;
        } else if (k_strcmp(start, "..") == 0) {
            if (top > 0) top--;
        } else {
            if (top < 32) stack[top++] = start;
        }
    }

    abspath[0] = '\0';
    if (top == 0) {
        k_strcpy(abspath, "/");
    } else {
        for (int i = 0; i < top; i++) {
            k_strncat(abspath, "/", 256);
            k_strncat(abspath, stack[i], 256);
        }
    }
}

int vfs_open(const char *name, int mode, u16 uid) {
    char abspath[256];
    resolve_path(name, abspath);
    return innofs_open(abspath, mode, uid);
}

int vfs_read(int fd, void *buf, u32 count) {
    return innofs_read(fd, buf, count);
}

int vfs_write(int fd, const void *buf, u32 count) {
    return innofs_write(fd, buf, count);
}

void vfs_close(int fd) {
    innofs_close(fd);
}

int vfs_rm(const char *name, u16 uid) {
    char abspath[256];
    resolve_path(name, abspath);
    return innofs_rm(abspath, uid);
}

int vfs_mkdir(const char *name, u16 owner_uid, u8 flags) {
    char abspath[256];
    resolve_path(name, abspath);
    return innofs_mkdir(abspath, owner_uid, flags);
}

int vfs_readdir(const char *dir_path, int index, vfs_dirent_t *dirent) {
    char abspath[256];
    if (!dir_path || dir_path[0] == '\0') {
        k_strncpy(abspath, g_cwd, 256);
    } else {
        resolve_path(dir_path, abspath);
    }
    return innofs_readdir(abspath, index, dirent);
}

void vfs_set_cwd(const char *path) {
    if (!path) return;
    char temp[256];
    resolve_path(path, temp);
    int len = k_strlen(temp);
    if (len > 1 && temp[len-1] == '/') {
        temp[len-1] = '\0';
    }
    k_strncpy(g_cwd, temp, sizeof(g_cwd));
}

const char *vfs_get_cwd(void) {
    return g_cwd;
}
