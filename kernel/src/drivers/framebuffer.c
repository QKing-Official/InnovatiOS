#include <kernel/drivers/framebuffer.h>
#include <kernel/lib/string.h>
#include <limine.h>

static framebuffer_t g_fb;

static u8 g_back_buffer[FB_MAX_WIDTH * FB_MAX_HEIGHT * 4];
static u64 g_back_pitch;

static u32 fb_blend_channel(u32 src, u32 dst, u32 alpha) {
    return (src * alpha + dst * (255u - alpha)) / 255u;
}

void fb_init(void *limine_fb_response) {
    struct limine_framebuffer_response *resp = limine_fb_response;
    struct limine_framebuffer *lfb = resp->framebuffers[0];

    g_fb.addr   = (u8 *)lfb->address;
    g_fb.width  = lfb->width;
    g_fb.height = lfb->height;
    g_fb.pitch  = lfb->pitch;
    g_fb.bpp    = lfb->bpp;

    if (g_fb.width  > FB_MAX_WIDTH)  g_fb.width  = FB_MAX_WIDTH;
    if (g_fb.height > FB_MAX_HEIGHT) g_fb.height = FB_MAX_HEIGHT;

    g_back_pitch = g_fb.width * 4;
    k_memset(g_back_buffer, 0, sizeof(g_back_buffer));
}

u64 fb_width(void)  { return g_fb.width; }
u64 fb_height(void) { return g_fb.height; }

void fb_put_pixel(u64 x, u64 y, u32 rgb) {
    if (x >= g_fb.width || y >= g_fb.height) return;
    u32 *pixel = (u32 *)(g_back_buffer + y * g_back_pitch + x * 4);
    *pixel = rgb;
}

void fb_clear(u32 rgb) {
    fb_fill_rect(0, 0, g_fb.width, g_fb.height, rgb);
}

void fb_fill_rect(u64 x, u64 y, u64 w, u64 h, u32 rgb) {
    for (u64 row = y; row < y + h && row < g_fb.height; row++) {
        for (u64 col = x; col < x + w && col < g_fb.width; col++) {
            u32 *pixel = (u32 *)(g_back_buffer + row * g_back_pitch + col * 4);
            *pixel = rgb;
        }
    }
}

void fb_draw_image(u64 x, u64 y, const fb_image_t *image, u64 size) {
    if (image == NULL || image->pixels == NULL || image->width == 0 || image->height == 0 || size == 0) {
        return;
    }

    u64 scaled_width = image->width * size;
    u64 scaled_height = image->height * size;

    for (u64 dst_row = 0; dst_row < scaled_height; dst_row++) {
        u64 dst_y = y + dst_row;
        if (dst_y >= g_fb.height) {
            break;
        }

        u64 src_row = dst_row / size;

        for (u64 dst_col = 0; dst_col < scaled_width; dst_col++) {
            u64 dst_x = x + dst_col;
            if (dst_x >= g_fb.width) {
                break;
            }

            u64 src_col = dst_col / size;

            u32 src = image->pixels[src_row * image->width + src_col];
            u32 alpha = src >> 24;

            if (alpha == 0) {
                continue;
            }

            u32 *dst = (u32 *)(g_back_buffer + dst_y * g_back_pitch + dst_x * 4);
            if (alpha == 255u) {
                *dst = 0xFF000000u | (src & 0x00FFFFFFu);
                continue;
            }

            u32 dst_px = *dst;
            u32 src_r = (src >> 16) & 0xFFu;
            u32 src_g = (src >> 8) & 0xFFu;
            u32 src_b = src & 0xFFu;
            u32 dst_r = (dst_px >> 16) & 0xFFu;
            u32 dst_g = (dst_px >> 8) & 0xFFu;
            u32 dst_b = dst_px & 0xFFu;

            u32 out_r = fb_blend_channel(src_r, dst_r, alpha);
            u32 out_g = fb_blend_channel(src_g, dst_g, alpha);
            u32 out_b = fb_blend_channel(src_b, dst_b, alpha);

            *dst = 0xFF000000u | (out_r << 16) | (out_g << 8) | out_b;
        }
    }
}

void fb_present(void) {
    for (u64 row = 0; row < g_fb.height; row++) {
        k_memcpy(g_fb.addr + row * g_fb.pitch,
                 g_back_buffer + row * g_back_pitch,
                 g_fb.width * 4);
    }
}
