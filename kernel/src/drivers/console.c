#include <kernel/drivers/console.h>
#include <kernel/drivers/framebuffer.h>
#include <kernel/drivers/keyboard.h>
#include <kernel/assets/font8x16.h>
#include <kernel/lib/string.h>

#define CON_MAX_COLS 240
#define CON_MAX_ROWS  67

static u64 g_cols;
static u64 g_rows;
static u64 g_cur_col;
static u64 g_cur_row;
static u32 g_fg;
static u32 g_bg;

static void draw_char(u64 col, u64 row, char c, u32 fg, u32 bg) {
    u64 px = col * FONT_CHAR_W;
    u64 py = row * FONT_CHAR_H;

    const u8 *glyph;
    if (c >= FONT_FIRST && c <= FONT_LAST)
        glyph = font8x16_glyphs[(int)c - FONT_FIRST];
    else
        glyph = font8x16_glyphs[0];

    for (u64 y = 0; y < FONT_CHAR_H; y++) {
        u8 bits = glyph[y];
        for (u64 x = 0; x < FONT_CHAR_W; x++) {
            u32 color = (bits & (0x80 >> x)) ? fg : bg;
            fb_put_pixel(px + x, py + y, color);
        }
    }
}

typedef struct {
    char ch;
    u32  fg;
    u32  bg;
} cell_t;

static cell_t g_cells[CON_MAX_ROWS][CON_MAX_COLS];

static void cells_clear(void) {
    for (u64 r = 0; r < g_rows; r++) {
        for (u64 c = 0; c < g_cols; c++) {
            g_cells[r][c].ch = ' ';
            g_cells[r][c].fg = g_fg;
            g_cells[r][c].bg = g_bg;
        }
    }
}

static void redraw_all(void) {
    fb_clear(g_bg);
    for (u64 r = 0; r < g_rows; r++) {
        for (u64 c = 0; c < g_cols; c++) {
            cell_t *cell = &g_cells[r][c];
            if (cell->ch != ' ' || cell->bg != g_bg) {
                draw_char(c, r, cell->ch, cell->fg, cell->bg);
            }
        }
    }
    fb_present();
}

static void scroll_up(void) {

    for (u64 r = 0; r < g_rows - 1; r++) {
        k_memcpy(g_cells[r], g_cells[r + 1], g_cols * sizeof(cell_t));
    }

    for (u64 c = 0; c < g_cols; c++) {
        g_cells[g_rows - 1][c].ch = ' ';
        g_cells[g_rows - 1][c].fg = g_fg;
        g_cells[g_rows - 1][c].bg = g_bg;
    }
    redraw_all();
}

void console_init(void) {
    g_cols = fb_width()  / FONT_CHAR_W;
    g_rows = fb_height() / FONT_CHAR_H;
    if (g_cols > CON_MAX_COLS) g_cols = CON_MAX_COLS;
    if (g_rows > CON_MAX_ROWS) g_rows = CON_MAX_ROWS;

    g_cur_col = 0;
    g_cur_row = 0;
    g_fg = CON_COLOR_WHITE;
    g_bg = CON_COLOR_BG;

    cells_clear();
    fb_clear(g_bg);
    fb_present();
}

void console_set_color(u32 fg, u32 bg) {
    g_fg = fg;
    g_bg = bg;
}

void console_set_cursor(u64 col, u64 row) {
    if (col < g_cols) g_cur_col = col;
    if (row < g_rows) g_cur_row = row;
}

void console_clear(void) {
    g_cur_col = 0;
    g_cur_row = 0;
    cells_clear();
    fb_clear(g_bg);
    fb_present();
}

void console_putc(char c) {
    if (c == '\n') {
        g_cur_col = 0;
        g_cur_row++;
        if (g_cur_row >= g_rows) {
            g_cur_row = g_rows - 1;
            scroll_up();
        }
        return;
    }

    if (c == '\b') {
        if (g_cur_col > 0) {
            g_cur_col--;
            g_cells[g_cur_row][g_cur_col].ch = ' ';
            g_cells[g_cur_row][g_cur_col].fg = g_fg;
            g_cells[g_cur_row][g_cur_col].bg = g_bg;
            draw_char(g_cur_col, g_cur_row, ' ', g_fg, g_bg);
            fb_present();
        }
        return;
    }

    if (c == '\t') {
        u64 spaces = 4 - (g_cur_col % 4);
        for (u64 i = 0; i < spaces; i++) console_putc(' ');
        return;
    }

    if (g_cur_col >= g_cols) {
        g_cur_col = 0;
        g_cur_row++;
        if (g_cur_row >= g_rows) {
            g_cur_row = g_rows - 1;
            scroll_up();
        }
    }

    g_cells[g_cur_row][g_cur_col].ch = c;
    g_cells[g_cur_row][g_cur_col].fg = g_fg;
    g_cells[g_cur_row][g_cur_col].bg = g_bg;
    draw_char(g_cur_col, g_cur_row, c, g_fg, g_bg);

    g_cur_col++;
}

void console_puts(const char *s) {
    while (*s) {
        console_putc(*s++);
    }
    fb_present();
}

void console_puts_color(const char *s, u32 fg) {
    u32 old_fg = g_fg;
    g_fg = fg;
    console_puts(s);
    g_fg = old_fg;
}

void console_readline(char *buf, size_t len, char echo_mask) {
    size_t pos = 0;
    k_memset(buf, 0, len);

    while (1) {
        char c = keyboard_read_char();

        if (c == '\n') {
            buf[pos] = '\0';
            console_putc('\n');
            return;
        }

        if (c == '\b') {
            if (pos > 0) {
                pos--;
                buf[pos] = '\0';
                console_putc('\b');
            }
            continue;
        }

        if (pos < len - 1 && c >= ' ') {
            buf[pos++] = c;
            if (echo_mask) {
                console_putc(echo_mask);
            } else {
                console_putc(c);
            }
            fb_present();
        }
    }
}

