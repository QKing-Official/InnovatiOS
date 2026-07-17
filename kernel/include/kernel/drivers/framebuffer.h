#ifndef KERNEL_DRIVERS_FRAMEBUFFER_H
#define KERNEL_DRIVERS_FRAMEBUFFER_H

#include <kernel/types.h>

typedef struct {
    u8  *addr;
    u64  width;
    u64  height;
    u64  pitch;
    u16  bpp;
} framebuffer_t;

typedef struct {
    const u32 *pixels;
    u64 width;
    u64 height;
} fb_image_t;

void fb_init(void *limine_fb_response);

u64 fb_width(void);
u64 fb_height(void);

void fb_put_pixel(u64 x, u64 y, u32 rgb);
void fb_clear(u32 rgb);
void fb_fill_rect(u64 x, u64 y, u64 w, u64 h, u32 rgb);
void fb_draw_image(u64 x, u64 y, const fb_image_t *image, u64 size);

void fb_present(void);

#define FB_MAX_WIDTH  1920
#define FB_MAX_HEIGHT 1080

#endif
