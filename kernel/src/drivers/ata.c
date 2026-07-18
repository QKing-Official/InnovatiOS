#include <kernel/drivers/ata.h>
#include <kernel/arch/x86_64/io.h>
#include <kernel/drivers/serial.h>

#define ATA_REG_DATA       0x00
#define ATA_REG_ERROR      0x01
#define ATA_REG_FEATURES   0x01
#define ATA_REG_SECCOUNT0  0x02
#define ATA_REG_LBA0       0x03
#define ATA_REG_LBA1       0x04
#define ATA_REG_LBA2       0x05
#define ATA_REG_HDDEVSEL   0x06
#define ATA_REG_COMMAND    0x07
#define ATA_REG_STATUS     0x07

#define ATA_CMD_READ_PIO   0x20
#define ATA_CMD_WRITE_PIO  0x30
#define ATA_CMD_CACHE_FLUSH 0xE7
#define ATA_CMD_IDENTIFY   0xEC

#define ATA_SR_BSY         0x80    // Busy
#define ATA_SR_DRDY        0x40    // Drive ready
#define ATA_SR_DF          0x20    // Drive write fault
#define ATA_SR_DSC         0x10    // Drive seek complete
#define ATA_SR_DRQ         0x08    // Data request ready
#define ATA_SR_CORR        0x04    // Corrected data
#define ATA_SR_IDX         0x02    // Index
#define ATA_SR_ERR         0x01    // Error

static int ata_wait_bsy(void) {
    for (int i = 0; i < 100000; i++) {
        u8 status = inb(ATA_PRIMARY_IO + ATA_REG_STATUS);
        if (status == 0xFF) return -1; // Floating bus
        if (!(status & ATA_SR_BSY)) return 0;
        io_wait();
    }
    return -1;
}

static int ata_wait_drq(void) {
    for (int i = 0; i < 100000; i++) {
        u8 status = inb(ATA_PRIMARY_IO + ATA_REG_STATUS);
        if (status == 0xFF) return -1;
        if (status & ATA_SR_ERR) return -1;
        if (status & ATA_SR_DRQ) return 0;
        io_wait();
    }
    return -1;
}

void ata_init(void) {
    serial_write("ata: initializing primary IDE channel...\n");
    // Just a basic init/detect could go here
    // For now we assume the master drive is present on the primary bus.
}

int ata_read_sectors(u32 lba, u8 sector_count, void *buffer) {
    if (ata_wait_bsy() != 0) return -1;

    outb(ATA_PRIMARY_IO + ATA_REG_HDDEVSEL, 0xE0 | ((lba >> 24) & 0x0F));
    outb(ATA_PRIMARY_IO + ATA_REG_FEATURES, 0x00);
    outb(ATA_PRIMARY_IO + ATA_REG_SECCOUNT0, sector_count);
    outb(ATA_PRIMARY_IO + ATA_REG_LBA0, (u8)lba);
    outb(ATA_PRIMARY_IO + ATA_REG_LBA1, (u8)(lba >> 8));
    outb(ATA_PRIMARY_IO + ATA_REG_LBA2, (u8)(lba >> 16));
    outb(ATA_PRIMARY_IO + ATA_REG_COMMAND, ATA_CMD_READ_PIO);

    u16 *buf16 = (u16 *)buffer;
    int words = 256; // 512 bytes per sector / 2 bytes per word
    
    for (int i = 0; i < sector_count; i++) {
        if (ata_wait_bsy() != 0) return -1;
        if (ata_wait_drq() != 0) return -1;
        insw(ATA_PRIMARY_IO + ATA_REG_DATA, buf16, words);
        buf16 += words;
    }
    
    return 0; // success
}

int ata_write_sectors(u32 lba, u8 sector_count, const void *buffer) {
    if (ata_wait_bsy() != 0) return -1;

    outb(ATA_PRIMARY_IO + ATA_REG_HDDEVSEL, 0xE0 | ((lba >> 24) & 0x0F));
    outb(ATA_PRIMARY_IO + ATA_REG_FEATURES, 0x00);
    outb(ATA_PRIMARY_IO + ATA_REG_SECCOUNT0, sector_count);
    outb(ATA_PRIMARY_IO + ATA_REG_LBA0, (u8)lba);
    outb(ATA_PRIMARY_IO + ATA_REG_LBA1, (u8)(lba >> 8));
    outb(ATA_PRIMARY_IO + ATA_REG_LBA2, (u8)(lba >> 16));
    outb(ATA_PRIMARY_IO + ATA_REG_COMMAND, ATA_CMD_WRITE_PIO);

    const u16 *buf16 = (const u16 *)buffer;
    int words = 256;
    
    for (int i = 0; i < sector_count; i++) {
        if (ata_wait_bsy() != 0) return -1;
        if (ata_wait_drq() != 0) return -1;
        outsw(ATA_PRIMARY_IO + ATA_REG_DATA, buf16, words);
        buf16 += words;
    }

    // Flush cache
    outb(ATA_PRIMARY_IO + ATA_REG_COMMAND, ATA_CMD_CACHE_FLUSH);
    ata_wait_bsy();

    return 0; // success
}
