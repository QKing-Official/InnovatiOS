#ifndef KERNEL_FS_INNOFS_H
#define KERNEL_FS_INNOFS_H

#include <kernel/fs/vfs.h>
#include <kernel/types.h>

// This is the InnoFS header file
// This file contains the methods that can be called to interact with this filesystem
// InnoFS is different from other major filesystems in that it is a very simple filesystem that is designed to be used in
// my special, selfwritten operating system called InnovatiOS
// This FS is not disigned to be used for real purposes, this is just a hobby filesystem that I have made for stardance hackclub

int innofs_init(void);
int innofs_mkfs(void);
int innofs_open(const char *name, int mode, u16 uid);
int innofs_read(int fd, void *buf, u32 count);
int innofs_write(int fd, const void *buf, u32 count);
void innofs_close(int fd);
int innofs_rm(const char *name, u16 uid);
int innofs_mkdir(const char *name, u16 owner_uid, u8 flags);
int innofs_readdir(const char *dir_path, int index, vfs_dirent_t *dirent);

#endif
