#ifndef KERNEL_FS_VFS_H
#define KERNEL_FS_VFS_H

#include <kernel/types.h>

#define VFS_FLAG_FILE       0x01
#define VFS_FLAG_DIRECTORY  0x02

#define VFS_MODE_READ       0x01
#define VFS_MODE_WRITE      0x02
#define VFS_MODE_CREATE     0x04

typedef struct {
    char name[116];
    u32 size;
    u32 flags;
    u16 owner_uid;
    u32 inode; // internal usage
} vfs_dirent_t;

// Initialize the VFS and the underlying file system
int vfs_init(void);

// Format the underlying file system
int vfs_mkfs(void);

// File operations
int vfs_open(const char *name, int mode, u16 uid);
int vfs_read(int fd, void *buf, u32 count);
int vfs_write(int fd, const void *buf, u32 count);
void vfs_close(int fd);
int vfs_rm(const char *name, u16 uid);

// Directory operations
int vfs_mkdir(const char *name, u16 owner_uid, u8 flags);
int vfs_readdir(const char *dir_path, int index, vfs_dirent_t *dirent);

// CWD operations
void vfs_set_cwd(const char *path);
const char *vfs_get_cwd(void);

#endif
