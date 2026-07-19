#include <kernel/fs/innofs.h>
#include <kernel/drivers/ata.h>
#include <kernel/drivers/serial.h>
#include <kernel/lib/string.h>
#include <kernel/mm/heap.h>

#define INNOFS_MAGIC "InnoFS Alpha"
#define SECTOR_SIZE  512

struct innofs_superblock {
    char magic[16];
    u32 disk_size_sectors;
    u32 bitmap_start_sector;
    u32 bitmap_sectors;
    u32 root_dir_start_sector;
    u32 root_dir_sectors;
    u32 data_start_sector;
    u8  padding[472];
} __attribute__((packed));

struct innofs_dirent {
    char name[116];
    u32 size;
    u32 first_sector;
    u16 owner_uid;
    u8 flags; // 0x01: is_dir, 0x02: root_only, 0x04: owner_only
} __attribute__((packed));

struct innofs_data_sector {
    u32 next_sector;
    u8 data[508];
} __attribute__((packed));

#define MAX_OPEN_FILES 16
typedef struct {
    bool used;
    u32 dirent_sector;
    u32 dirent_index;
    u32 offset;
    u32 size;
    u32 first_sector;
    int mode;
} open_file_t;

static struct innofs_superblock g_sb;
static open_file_t g_open_files[MAX_OPEN_FILES];
static bool g_innofs_mounted = false;

static int alloc_fd(void) {
    for (int i = 0; i < MAX_OPEN_FILES; i++) {
        if (!g_open_files[i].used) {
            g_open_files[i].used = true;
            return i;
        }
    }
    return -1;
}

static u32 bitmap_alloc_sector(void) {
    u8 *bitmap = kmalloc(SECTOR_SIZE);
    for (u32 i = 0; i < g_sb.bitmap_sectors; i++) {
        ata_read_sectors(g_sb.bitmap_start_sector + i, 1, bitmap);
        for (u32 byte = 0; byte < SECTOR_SIZE; byte++) {
            if (bitmap[byte] != 0xFF) {
                for (u8 bit = 0; bit < 8; bit++) {
                    if (!(bitmap[byte] & (1 << bit))) {
                        bitmap[byte] |= (1 << bit);
                        ata_write_sectors(g_sb.bitmap_start_sector + i, 1, bitmap);
                        u32 sector = (i * SECTOR_SIZE * 8) + (byte * 8) + bit;
                        kfree(bitmap);
                        return sector;
                    }
                }
            }
        }
    }
    kfree(bitmap);
    return 0;
}

static void bitmap_free_sector(u32 sector) {
    if (sector < g_sb.data_start_sector) return;
    u32 bit_idx = sector;
    u32 bitmap_sector = bit_idx / (SECTOR_SIZE * 8);
    u32 byte_idx = (bit_idx % (SECTOR_SIZE * 8)) / 8;
    u32 bit_rem = bit_idx % 8;

    u8 *bitmap = kmalloc(SECTOR_SIZE);
    ata_read_sectors(g_sb.bitmap_start_sector + bitmap_sector, 1, bitmap);
    bitmap[byte_idx] &= ~(1 << bit_rem);
    ata_write_sectors(g_sb.bitmap_start_sector + bitmap_sector, 1, bitmap);
    kfree(bitmap);
}

static bool check_access(struct innofs_dirent *dirent, u16 uid) {
    if (uid == 0) return true;
    if (dirent->flags & 0x02) return false;
    if (dirent->flags & 0x04) return (dirent->owner_uid == uid);
    return true;
}

static int split_path(const char *path, char *parent_dir, char *filename) {
    int last_slash = -1;
    for (int i = 0; path[i]; i++) {
        if (path[i] == '/') last_slash = i;
    }
    if (last_slash == -1) return -1;
    if (last_slash == 0) {
        k_strcpy(parent_dir, "/");
    } else {
        k_strncpy(parent_dir, path, last_slash);
        parent_dir[last_slash] = '\0';
    }
    k_strcpy(filename, path + last_slash + 1);
    return 0;
}

static int find_dirent(const char *path, u32 *out_dir_sector, u32 *out_index, struct innofs_dirent *out_dirent, bool *out_in_root) {
    if (path[0] != '/') return -1;
    if (k_strcmp(path, "/") == 0) {
        k_memset(out_dirent, 0, sizeof(*out_dirent));
        out_dirent->flags = 1; 
        out_dirent->first_sector = g_sb.root_dir_start_sector;
        if (out_in_root) *out_in_root = true;
        return 0;
    }

    u32 current_dir_sector = g_sb.root_dir_start_sector;
    bool in_root = true;
    const char *p = path + 1;
    char token[116];
    struct innofs_dirent *entries = kmalloc(SECTOR_SIZE);
    struct innofs_data_sector *sec = kmalloc(SECTOR_SIZE);

    while (*p) {
        int i = 0;
        while (*p && *p != '/' && i < 115) {
            token[i++] = *p++;
        }
        token[i] = '\0';
        if (*p == '/') p++;

        if (!in_root) {
        }

        bool found = false;
        if (in_root) {
            for (u32 s = 0; s < g_sb.root_dir_sectors; s++) {
                ata_read_sectors(current_dir_sector + s, 1, entries);
                for (int j = 0; j < 4; j++) {
                    if (entries[j].name[0] != 0 && k_strcmp(entries[j].name, token) == 0) {
                        found = true;
                        if (out_dir_sector) *out_dir_sector = current_dir_sector + s;
                        if (out_index) *out_index = j;
                        if (out_dirent) k_memcpy(out_dirent, &entries[j], sizeof(*out_dirent));
                        if (out_in_root) *out_in_root = false;
                        current_dir_sector = entries[j].first_sector;
                        break;
                    }
                }
                if (found) break;
            }
        } else {
            u32 sec_idx = current_dir_sector;
            while (sec_idx != 0) {
                ata_read_sectors(sec_idx, 1, sec);
                struct innofs_dirent *dir_entries = (struct innofs_dirent *)sec->data;
                for (int j = 0; j < 4; j++) {
                    if (dir_entries[j].name[0] != 0 && k_strcmp(dir_entries[j].name, token) == 0) {
                        found = true;
                        if (out_dir_sector) *out_dir_sector = sec_idx;
                        if (out_index) *out_index = j;
                        if (out_dirent) k_memcpy(out_dirent, &dir_entries[j], sizeof(*out_dirent));
                        if (out_in_root) *out_in_root = false;
                        current_dir_sector = dir_entries[j].first_sector;
                        break;
                    }
                }
                if (found) break;
                sec_idx = sec->next_sector;
            }
        }
        if (!found) { kfree(entries); kfree(sec); return -1; }
        in_root = false;
    }
    kfree(entries);
    kfree(sec);
    return 0;
}

static int add_dirent(u32 dir_sector, bool in_root, const char *name, u16 owner_uid, u8 flags, u32 *out_dir_sector, u32 *out_index) {
    struct innofs_data_sector *sec = kmalloc(SECTOR_SIZE);
    struct innofs_dirent *entries = kmalloc(SECTOR_SIZE);
    
    // Check if name already exists
    if (in_root) {
        for (u32 s = 0; s < g_sb.root_dir_sectors; s++) {
            ata_read_sectors(dir_sector + s, 1, entries);
            for (int j = 0; j < 4; j++) {
                if (entries[j].name[0] != 0 && k_strcmp(entries[j].name, name) == 0) {
                    kfree(sec); kfree(entries); return -1;
                }
            }
        }
    } else {
        u32 sec_idx = dir_sector;
        while (sec_idx != 0) {
            ata_read_sectors(sec_idx, 1, sec);
            struct innofs_dirent *dir_entries = (struct innofs_dirent *)sec->data;
            for (int j = 0; j < 4; j++) {
                if (dir_entries[j].name[0] != 0 && k_strcmp(dir_entries[j].name, name) == 0) {
                    kfree(sec); kfree(entries); return -1;
                }
            }
            sec_idx = sec->next_sector;
        }
    }

    
    if (in_root) {
        for (u32 s = 0; s < g_sb.root_dir_sectors; s++) {
            ata_read_sectors(dir_sector + s, 1, entries);
            for (int j = 0; j < 4; j++) {
                if (entries[j].name[0] == 0) {
                    k_strncpy(entries[j].name, name, 115);
                    entries[j].name[115] = '\0';
                    entries[j].size = 0;
                    entries[j].first_sector = 0;
                    entries[j].owner_uid = owner_uid;
                    entries[j].flags = flags;
                    ata_write_sectors(dir_sector + s, 1, entries);
                    *out_dir_sector = dir_sector + s;
                    *out_index = j;
                    kfree(sec); kfree(entries); return 0;
                }
            }
        }
    } else {
        u32 sec_idx = dir_sector;
        u32 last_sec = 0;
        while (sec_idx != 0) {
            ata_read_sectors(sec_idx, 1, sec);
            struct innofs_dirent *dir_entries = (struct innofs_dirent *)sec->data;
            for (int j = 0; j < 4; j++) {
                if (dir_entries[j].name[0] == 0) {
                    k_strncpy(dir_entries[j].name, name, 115);
                    dir_entries[j].name[115] = '\0';
                    dir_entries[j].size = 0;
                    dir_entries[j].first_sector = 0;
                    dir_entries[j].owner_uid = owner_uid;
                    dir_entries[j].flags = flags;
                    ata_write_sectors(sec_idx, 1, sec);
                    *out_dir_sector = sec_idx;
                    *out_index = j;
                    kfree(sec); kfree(entries); return 0;
                }
            }
            last_sec = sec_idx;
            sec_idx = sec->next_sector;
        }
        
        u32 new_sec = bitmap_alloc_sector();
        if (new_sec == 0) { kfree(sec); kfree(entries); return -1; }
        
        ata_read_sectors(last_sec, 1, sec);
        sec->next_sector = new_sec;
        ata_write_sectors(last_sec, 1, sec);
        
        k_memset(sec, 0, SECTOR_SIZE);
        struct innofs_dirent *dir_entries = (struct innofs_dirent *)sec->data;
        k_strncpy(dir_entries[0].name, name, 115);
        dir_entries[0].name[115] = '\0';
        dir_entries[0].size = 0;
        dir_entries[0].first_sector = 0;
        dir_entries[0].owner_uid = owner_uid;
        dir_entries[0].flags = flags;
        ata_write_sectors(new_sec, 1, sec);
        
        *out_dir_sector = new_sec;
        *out_index = 0;
        kfree(sec); kfree(entries); return 0;
    }
    kfree(sec); kfree(entries); return -1;
}

int innofs_init(void) {
    ata_read_sectors(0, 1, &g_sb);
    if (k_strncmp(g_sb.magic, INNOFS_MAGIC, 12) == 0) {
        g_innofs_mounted = true;
        serial_write("innofs: mounted successfully\n");
        return 0;
    }
    serial_write("innofs: no valid filesystem found, auto-formatting...\n");
    innofs_mkfs();
    return 1;
}

int innofs_mkfs(void) {
    k_memset(&g_sb, 0, SECTOR_SIZE);
    k_strncpy(g_sb.magic, "InnoFS Alpha", 16);
    g_sb.disk_size_sectors = 65536;
    g_sb.bitmap_start_sector = 1;
    g_sb.bitmap_sectors = 2;
    g_sb.root_dir_start_sector = 3;
    g_sb.root_dir_sectors = 16;
    g_sb.data_start_sector = 19;

    ata_write_sectors(0, 1, &g_sb);

    u8 *zero_sector = kmalloc(SECTOR_SIZE);
    k_memset(zero_sector, 0, SECTOR_SIZE);
    for (u32 i = 0; i < g_sb.bitmap_sectors; i++) {
        ata_write_sectors(g_sb.bitmap_start_sector + i, 1, zero_sector);
    }
    
    for (u32 i = 0; i < g_sb.data_start_sector; i++) {
        zero_sector[i / 8] |= (1 << (i % 8));
    }
    ata_write_sectors(g_sb.bitmap_start_sector, 1, zero_sector);
    k_memset(zero_sector, 0, SECTOR_SIZE);

    for (u32 i = 0; i < g_sb.root_dir_sectors; i++) {
        ata_write_sectors(g_sb.root_dir_start_sector + i, 1, zero_sector);
    }
    kfree(zero_sector);
    
    g_innofs_mounted = true;
    
    // Create default directories
    innofs_mkdir("/root", 0, 0x02); // root_only
    innofs_mkdir("/root/global", 0, 0x02);
    innofs_mkdir("/home", 0, 0); // public
    // Guest/Root dirs are auto-created by user_init()


    serial_write("innofs: formatted successfully\n");
    return 0;
}

int innofs_open(const char *name, int mode, u16 uid) {
    if (!g_innofs_mounted) return -1;
    
    u32 dir_sector, index;
    struct innofs_dirent dirent;
    bool in_root;
    
    if (find_dirent(name, &dir_sector, &index, &dirent, &in_root) == 0) {
        if (!check_access(&dirent, uid)) return -1;
        if (dirent.flags & 0x01) return -1; // Cannot open directory as file
        
        if (mode & VFS_MODE_CREATE) {
            dirent.size = 0;
            struct innofs_dirent *entries = kmalloc(SECTOR_SIZE);
            ata_read_sectors(dir_sector, 1, entries);
            entries[index].size = 0;
            ata_write_sectors(dir_sector, 1, entries);
            kfree(entries);
        }
        
        int fd = alloc_fd();
        if (fd >= 0) {
            g_open_files[fd].dirent_sector = dir_sector;
            g_open_files[fd].dirent_index = index;
            g_open_files[fd].offset = 0;
            g_open_files[fd].size = dirent.size;
            g_open_files[fd].first_sector = dirent.first_sector;
            g_open_files[fd].mode = mode;
        }
        return fd;
    }
    
    if (!(mode & VFS_MODE_CREATE)) return -1;
    
    char parent_dir[256];
    char filename[116];
    if (split_path(name, parent_dir, filename) != 0) return -1;
    
    u32 p_dir_sector, p_index;
    struct innofs_dirent parent_dirent;
    bool p_in_root;
    if (find_dirent(parent_dir, &p_dir_sector, &p_index, &parent_dirent, &p_in_root) != 0) return -1;
    if (!(parent_dirent.flags & 0x01)) return -1; // Parent is not a directory
    if (!check_access(&parent_dirent, uid)) return -1; // Cannot create file in restricted dir
    
    if (add_dirent(parent_dirent.first_sector, p_in_root, filename, uid, 0, &dir_sector, &index) == 0) {
        if (parent_dirent.first_sector == 0) {
            struct innofs_dirent *entries = kmalloc(SECTOR_SIZE);
            ata_read_sectors(p_dir_sector, 1, entries);
            entries[p_index].first_sector = dir_sector;
            ata_write_sectors(p_dir_sector, 1, entries);
            kfree(entries);
        }
        int fd = alloc_fd();
        if (fd >= 0) {
            g_open_files[fd].dirent_sector = dir_sector;
            g_open_files[fd].dirent_index = index;
            g_open_files[fd].offset = 0;
            g_open_files[fd].size = 0;
            g_open_files[fd].first_sector = 0;
            g_open_files[fd].mode = mode;
        }
        return fd;
    }
    return -1;
}

int innofs_mkdir(const char *name, u16 owner_uid, u8 flags) {
    if (!g_innofs_mounted) return -1;
    
    char parent_dir[256];
    char dirname[116];
    if (split_path(name, parent_dir, dirname) != 0) return -1;
    
    u32 p_dir_sector, p_index;
    struct innofs_dirent parent_dirent;
    bool p_in_root;
    if (find_dirent(parent_dir, &p_dir_sector, &p_index, &parent_dirent, &p_in_root) != 0) return -1;
    if (!(parent_dirent.flags & 0x01)) return -1;
    if (!check_access(&parent_dirent, owner_uid)) return -1;
    
    u32 out_sec, out_idx;
    int ret = add_dirent(parent_dirent.first_sector, p_in_root, dirname, owner_uid, flags | 0x01, &out_sec, &out_idx);
    if (ret == 0 && parent_dirent.first_sector == 0) {
        struct innofs_dirent *entries = kmalloc(SECTOR_SIZE);
        ata_read_sectors(p_dir_sector, 1, entries);
        entries[p_index].first_sector = out_sec;
        ata_write_sectors(p_dir_sector, 1, entries);
        kfree(entries);
    }
    return ret;
}

int innofs_read(int fd, void *buf, u32 count) {
    if (fd < 0 || fd >= MAX_OPEN_FILES || !g_open_files[fd].used) return -1;
    open_file_t *f = &g_open_files[fd];
    if (!(f->mode & VFS_MODE_READ)) return -1;

    u32 read_bytes = 0;
    u8 *out = (u8 *)buf;
    struct innofs_data_sector *sec = kmalloc(SECTOR_SIZE);
    u32 current_sec = f->first_sector;

    u32 skip_sectors = f->offset / 508;
    for (u32 i = 0; i < skip_sectors && current_sec != 0; i++) {
        ata_read_sectors(current_sec, 1, sec);
        current_sec = sec->next_sector;
    }

    while (count > 0 && current_sec != 0 && f->offset < f->size) {
        ata_read_sectors(current_sec, 1, sec);
        u32 sec_offset = f->offset % 508;
        u32 to_read = 508 - sec_offset;
        if (to_read > count) to_read = count;
        if (to_read > f->size - f->offset) to_read = f->size - f->offset;

        k_memcpy(out, sec->data + sec_offset, to_read);
        out += to_read;
        read_bytes += to_read;
        count -= to_read;
        f->offset += to_read;

        if (sec_offset + to_read == 508) {
            current_sec = sec->next_sector;
        }
    }
    kfree(sec);
    return read_bytes;
}

int innofs_write(int fd, const void *buf, u32 count) {
    if (fd < 0 || fd >= MAX_OPEN_FILES || !g_open_files[fd].used) return -1;
    open_file_t *f = &g_open_files[fd];
    if (!(f->mode & VFS_MODE_WRITE)) return -1;

    u32 written = 0;
    const u8 *in = (const u8 *)buf;
    struct innofs_data_sector *sec = kmalloc(SECTOR_SIZE);
    
    if (f->first_sector == 0) {
        f->first_sector = bitmap_alloc_sector();
        if (f->first_sector == 0) { kfree(sec); return 0; }
        k_memset(sec, 0, SECTOR_SIZE);
        ata_write_sectors(f->first_sector, 1, sec);
    }

    u32 current_sec = f->first_sector;
    u32 skip_sectors = f->offset / 508;
    
    for (u32 i = 0; i < skip_sectors; i++) {
        ata_read_sectors(current_sec, 1, sec);
        if (sec->next_sector == 0) {
            sec->next_sector = bitmap_alloc_sector();
            ata_write_sectors(current_sec, 1, sec);
            current_sec = sec->next_sector;
            k_memset(sec, 0, SECTOR_SIZE);
            ata_write_sectors(current_sec, 1, sec);
        } else {
            current_sec = sec->next_sector;
        }
    }

    while (count > 0) {
        ata_read_sectors(current_sec, 1, sec);
        u32 sec_offset = f->offset % 508;
        u32 to_write = 508 - sec_offset;
        if (to_write > count) to_write = count;

        k_memcpy(sec->data + sec_offset, in, to_write);
        ata_write_sectors(current_sec, 1, sec);
        
        in += to_write;
        written += to_write;
        count -= to_write;
        f->offset += to_write;
        if (f->offset > f->size) f->size = f->offset;

        if (sec_offset + to_write == 508 && count > 0) {
            if (sec->next_sector == 0) {
                sec->next_sector = bitmap_alloc_sector();
                ata_write_sectors(current_sec, 1, sec);
                current_sec = sec->next_sector;
                k_memset(sec, 0, SECTOR_SIZE);
                ata_write_sectors(current_sec, 1, sec);
            } else {
                current_sec = sec->next_sector;
            }
        }
    }
    kfree(sec);
    return written;
}

void innofs_close(int fd) {
    if (fd < 0 || fd >= MAX_OPEN_FILES || !g_open_files[fd].used) return;
    open_file_t *f = &g_open_files[fd];

    if (f->mode & (VFS_MODE_WRITE | VFS_MODE_CREATE)) {
        struct innofs_dirent *entries = kmalloc(SECTOR_SIZE);
        ata_read_sectors(f->dirent_sector, 1, entries);
        entries[f->dirent_index].size = f->size;
        entries[f->dirent_index].first_sector = f->first_sector;
        ata_write_sectors(f->dirent_sector, 1, entries);
        kfree(entries);
    }
    f->used = false;
}

int innofs_rm(const char *name, u16 uid) {
    if (!g_innofs_mounted) return -1;
    u32 dir_sector, index;
    struct innofs_dirent dirent;
    if (find_dirent(name, &dir_sector, &index, &dirent, NULL) != 0) return -1;
    if (!check_access(&dirent, uid)) return -1;
    
    u32 current_sec = dirent.first_sector;
    struct innofs_data_sector *sec = kmalloc(SECTOR_SIZE);
    while (current_sec != 0) {
        ata_read_sectors(current_sec, 1, sec);
        u32 next = sec->next_sector;
        bitmap_free_sector(current_sec);
        current_sec = next;
    }
    kfree(sec);

    struct innofs_dirent *entries = kmalloc(SECTOR_SIZE);
    ata_read_sectors(dir_sector, 1, entries);
    entries[index].name[0] = 0;
    ata_write_sectors(dir_sector, 1, entries);
    kfree(entries);
    return 0;
}

int innofs_readdir(const char *dir_path, int index, vfs_dirent_t *dirent) {
    if (!g_innofs_mounted) return 0;
    
    struct innofs_dirent target_dir;
    bool in_root;
    if (find_dirent(dir_path, NULL, NULL, &target_dir, &in_root) != 0) return 0;
    if (!(target_dir.flags & 0x01)) return 0;
    
    struct innofs_dirent *entries = kmalloc(SECTOR_SIZE);
    int current_index = 0;
    
    if (in_root) {
        for (u32 s = 0; s < g_sb.root_dir_sectors; s++) {
            ata_read_sectors(target_dir.first_sector + s, 1, entries);
            for (int j = 0; j < 4; j++) {
                if (entries[j].name[0] != 0) {
                    if (current_index == index) {
                        k_strncpy(dirent->name, entries[j].name, 115);
                        dirent->name[115] = '\0';
                        dirent->size = entries[j].size;
                        dirent->flags = (entries[j].flags & 0x01) ? VFS_FLAG_DIRECTORY : VFS_FLAG_FILE;
                        dirent->owner_uid = entries[j].owner_uid;
                        kfree(entries);
                        return 1;
                    }
                    current_index++;
                }
            }
        }
    } else {
        u32 sec_idx = target_dir.first_sector;
        struct innofs_data_sector *sec = kmalloc(SECTOR_SIZE);
        while (sec_idx != 0) {
            ata_read_sectors(sec_idx, 1, sec);
            struct innofs_dirent *dir_entries = (struct innofs_dirent *)sec->data;
            for (int j = 0; j < 4; j++) {
                if (dir_entries[j].name[0] != 0) {
                    if (current_index == index) {
                        k_strncpy(dirent->name, dir_entries[j].name, 115);
                        dirent->name[115] = '\0';
                        dirent->size = dir_entries[j].size;
                        dirent->flags = (dir_entries[j].flags & 0x01) ? VFS_FLAG_DIRECTORY : VFS_FLAG_FILE;
                        dirent->owner_uid = dir_entries[j].owner_uid;
                        kfree(entries);
                        kfree(sec);
                        return 1;
                    }
                    current_index++;
                }
            }
            sec_idx = sec->next_sector;
        }
        kfree(sec);
    }
    kfree(entries);
    return 0;
}
