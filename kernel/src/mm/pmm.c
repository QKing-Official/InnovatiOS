#include <kernel/mm/pmm.h>
#include <kernel/lib/string.h>
#include <kernel/drivers/serial.h>
#include <limine.h>

static u8 *bitmap;
static u64 total_pages;
static u64 free_pages_c;
static u64 hhdm;
static u64 scan_hint;

static inline void bitmap_set(u64 idx)   { bitmap[idx / 8] |= (u8)(1u << (idx % 8)); }
static inline void bitmap_clear(u64 idx) { bitmap[idx / 8] &= (u8)~(1u << (idx % 8)); }
static inline int  bitmap_test(u64 idx)  { return (bitmap[idx / 8] >> (idx % 8)) & 1; }

void pmm_init(void *limine_memmap_response, u64 hhdm_offset) {
    struct limine_memmap_response *memmap = limine_memmap_response;
    hhdm = hhdm_offset;

    u64 highest_addr = 0;
    for (u64 i = 0; i < memmap->entry_count; i++) {
        struct limine_memmap_entry *e = memmap->entries[i];
        u64 end = e->base + e->length;
        if (end > highest_addr) highest_addr = end;
    }

    total_pages = highest_addr / PMM_PAGE_SIZE;
    u64 bitmap_bytes = (total_pages + 7) / 8;

    u64 bitmap_phys = 0;
    for (u64 i = 0; i < memmap->entry_count; i++) {
        struct limine_memmap_entry *e = memmap->entries[i];
        if (e->type == LIMINE_MEMMAP_USABLE && e->length >= bitmap_bytes) {
            bitmap_phys = e->base;
            break;
        }
    }

    if (bitmap_phys == 0) {
        serial_write("pmm: FATAL - no usable region big enough for the bitmap\n");
        for (;;) { __asm__ volatile ("cli; hlt"); }
    }

    bitmap = (u8 *)(bitmap_phys + hhdm);

    k_memset(bitmap, 0xFF, bitmap_bytes);
    free_pages_c = 0;

    for (u64 i = 0; i < memmap->entry_count; i++) {
        struct limine_memmap_entry *e = memmap->entries[i];
        if (e->type != LIMINE_MEMMAP_USABLE) continue;

        u64 start_page = e->base / PMM_PAGE_SIZE;
        u64 page_count = e->length / PMM_PAGE_SIZE;
        for (u64 p = 0; p < page_count; p++) {
            bitmap_clear(start_page + p);
            free_pages_c++;
        }
    }

    u64 bitmap_start_page = bitmap_phys / PMM_PAGE_SIZE;
    u64 bitmap_page_count = (bitmap_bytes + PMM_PAGE_SIZE - 1) / PMM_PAGE_SIZE;
    for (u64 p = 0; p < bitmap_page_count; p++) {
        u64 idx = bitmap_start_page + p;
        if (!bitmap_test(idx)) {
            bitmap_set(idx);
            free_pages_c--;
        }
    }

    scan_hint = 0;
    serial_write("pmm: initialized\n");
}

u64 pmm_alloc_page(void) {
    for (u64 i = scan_hint; i < total_pages; i++) {
        if (!bitmap_test(i)) {
            bitmap_set(i);
            free_pages_c--;
            scan_hint = i + 1;
            return i * PMM_PAGE_SIZE;
        }
    }
    for (u64 i = 0; i < scan_hint && i < total_pages; i++) {
        if (!bitmap_test(i)) {
            bitmap_set(i);
            free_pages_c--;
            scan_hint = i + 1;
            return i * PMM_PAGE_SIZE;
        }
    }
    return 0;
}

u64 pmm_alloc_pages(u64 count) {
    if (count == 0 || count > total_pages) return 0;

    u64 start = 0;
    while (start + count <= total_pages) {
        u64 j = 0;
        while (j < count && !bitmap_test(start + j)) j++;

        if (j == count) {
            for (u64 k = 0; k < count; k++) bitmap_set(start + k);
            free_pages_c -= count;
            return start * PMM_PAGE_SIZE;
        }

        start = start + j + 1;
    }
    return 0;
}

void pmm_free_page(u64 phys_addr) {
    u64 idx = phys_addr / PMM_PAGE_SIZE;
    if (idx >= total_pages) return;
    if (bitmap_test(idx)) {
        bitmap_clear(idx);
        free_pages_c++;
        if (idx < scan_hint) scan_hint = idx;
    }
}

void pmm_free_pages(u64 phys_addr, u64 count) {
    u64 start = phys_addr / PMM_PAGE_SIZE;
    for (u64 i = 0; i < count; i++) {
        pmm_free_page((start + i) * PMM_PAGE_SIZE);
    }
}

u64 pmm_total_pages(void)      { return total_pages; }
u64 pmm_free_pages_count(void) { return free_pages_c; }
