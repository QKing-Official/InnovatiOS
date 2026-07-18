#ifndef KERNEL_DRIVERS_ATA_H
#define KERNEL_DRIVERS_ATA_H

#include <kernel/types.h>

#define ATA_PRIMARY_IO      0x1F0
#define ATA_PRIMARY_CTRL    0x3F6

void ata_init(void);
int ata_read_sectors(u32 lba, u8 sector_count, void *buffer);
int ata_write_sectors(u32 lba, u8 sector_count, const void *buffer);

#endif
