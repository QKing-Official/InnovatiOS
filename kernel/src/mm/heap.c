#include <kernel/mm/heap.h>
#include <kernel/mm/pmm.h>
#include <kernel/drivers/serial.h>

typedef struct block_header {
    size_t size;
    int    free;
    struct block_header *next;
} block_header_t;

#define HEADER_SIZE ((size_t)sizeof(block_header_t))
#define ALIGN8(x)   (((x) + 7ULL) & ~((size_t)7))

static block_header_t *heap_head = NULL;

void heap_init(u64 hhdm_offset, u64 initial_pages) {
    u64 phys = pmm_alloc_pages(initial_pages);
    if (phys == 0) {
        serial_write("heap: FATAL - could not reserve initial heap pages\n");
        for (;;) { __asm__ volatile ("cli; hlt"); }
    }

    heap_head = (block_header_t *)(phys + hhdm_offset);
    heap_head->size = initial_pages * PMM_PAGE_SIZE - HEADER_SIZE;
    heap_head->free = 1;
    heap_head->next = NULL;

    serial_write("heap: initialized\n");
}

void *kmalloc(size_t size) {
    if (size == 0) return NULL;
    size = ALIGN8(size);

    block_header_t *cur = heap_head;
    while (cur) {
        if (cur->free && cur->size >= size) {
            if (cur->size >= size + HEADER_SIZE + 8) {
                block_header_t *rest =
                    (block_header_t *)((u8 *)cur + HEADER_SIZE + size);
                rest->size = cur->size - size - HEADER_SIZE;
                rest->free = 1;
                rest->next = cur->next;

                cur->next = rest;
                cur->size = size;
            }
            cur->free = 0;
            return (void *)((u8 *)cur + HEADER_SIZE);
        }
        cur = cur->next;
    }

    return NULL;
}

void kfree(void *ptr) {
    if (!ptr) return;

    block_header_t *blk = (block_header_t *)((u8 *)ptr - HEADER_SIZE);
    blk->free = 1;

    if (blk->next && blk->next->free &&
        (u8 *)blk->next == (u8 *)blk + HEADER_SIZE + blk->size) {
        blk->size += HEADER_SIZE + blk->next->size;
        blk->next = blk->next->next;
    }
}
