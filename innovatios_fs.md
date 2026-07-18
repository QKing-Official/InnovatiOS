# InnoFS v2 Architecture and Permissions

InnovatiOS features a custom filesystem known as **InnoFS**. In its second iteration (v2), it has been completely rewritten to move away from a simple flat model into a powerful, hierarchical filesystem tailored specifically for the InnovatiOS user-space.

While taking inspiration from UNIX-like filesystems, InnoFS v2 simplifies many elements to optimize for performance, simplicity, and safety within a monolithic environment.

## 1. Disk Layout

The filesystem is divided into distinct sections on the disk, making metadata retrieval and data storage completely separated for speed:

1. **Superblock**: Resides in Sector 0. Contains magic numbers (`0x494E4E4F`), disk size, and offsets for other regions.
2. **Block Bitmap**: Resides right after the superblock. It tracks the usage of every data sector in the filesystem to ensure constant time O(1) free sector lookups.
3. **Root Directory Entries**: Fixed-size contiguous region to store the root directory (`/`) without needing sector chaining.
4. **Data Sectors**: The vast majority of the disk. These sectors store file contents AND sub-directory metadata in a linked-list sector fashion.

## 2. Directory Entries (Dirents)

Each directory entry takes up 128 bytes, allowing exactly **4 entries per 512-byte sector**. Unlike traditional inodes, InnoFS directory entries contain the metadata directly:
- `name`: Up to 115 characters.
- `size`: The size of the file in bytes (or 0 for directories).
- `first_sector`: A pointer to the first data sector.
- `owner_uid`: The User ID of the owner.
- `flags`: The most critical piece for our simplified permission system.

## 3. Simplified Permissions

Traditional Linux systems use complex read/write/execute bitmasks for owner/group/other. InnoFS uses a more elegant, flag-based approach that guarantees secure isolation without the mental overhead:

- **0x00 (Public)**: By default, if no flags are set, the file/directory is readable and writable by anyone.
- **0x01 (Directory)**: This entry is a directory and its data sectors contain other dirents.
- **0x02 (Root Only)**: The ultimate protection. Only `uid=0` (root) can read, write, or enter.
- **0x04 (Owner Only)**: The owner's privacy shield. Only `uid=0` (root) and `owner_uid` can access.

### Why is this better?
Instead of configuring groups and executable bits, InnoFS forces you to categorize data into three buckets: public, personal, or system-critical. It prevents misconfigurations and ensures that user home directories (`/home/<user>`) can be securely locked with a single `0x04` flag.

## 4. Path Resolution

InnoFS VFS layer implements a global Working Directory (`g_cwd`).
When a relative path like `dir/file.txt` is requested, the VFS automatically prefixes it with the CWD. Absolute paths start with `/` and traverse through `find_dirent` iteratively.

## 5. Storage Efficiency

Files in InnoFS are stored as linked sectors. Each 512-byte data sector reserves the first 4 bytes as a `next_sector` pointer, leaving 508 bytes for raw data. This linked-list architecture eliminates the need for complex extent trees and makes file appending extremely simple, although it trades off sequential read speed for implementation simplicity and low memory overhead.
