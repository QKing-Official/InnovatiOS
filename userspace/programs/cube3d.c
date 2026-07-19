#include <programs.h>
#include <kernel/drivers/framebuffer.h>

#if CONFIG_PIT
#include <kernel/drivers/pit.h>
#endif

#if CONFIG_KEYBOARD
#include <kernel/drivers/keyboard.h>
#endif

/* ---------------------------------------------------------------------
 * Minimal software 3D pipeline: a rotating wireframe cube.
 *
 * There is no libm in a freestanding kernel build, so sin/cos are
 * computed with Bhaskara I's integer sine approximation instead of
 * sinf()/cosf(). All trig results are fixed-point, scaled by 1000
 * ("permille"): sin1000(90) == 1000 means sin(90 deg) == 1.000.
 * --------------------------------------------------------------------- */

#define FP 1000  /* fixed-point scale used for trig + rotation math */

/* sin(x) for x in [0, 180] degrees, scaled by FP. Bhaskara I approximation:
 *   sin(x) ~= 4x(180-x) / (40500 - x(180-x))
 */
static i32 sin1000_0_180(i32 x) {
    i32 t = x * (180 - x);
    return (4 * t * FP) / (40500 - t);
}

/* Full-range sin, x in degrees (any integer, positive or negative). */
static i32 sin1000(i32 angle_deg) {
    i32 x = angle_deg % 360;
    if (x < 0) x += 360;

    if (x <= 180) {
        return sin1000_0_180(x);
    }
    return -sin1000_0_180(x - 180);
}

static i32 cos1000(i32 angle_deg) {
    return sin1000(angle_deg + 90);
}

/* ---------------------------------------------------------------------
 * Cube geometry: 8 vertices, 12 edges, in plain integer "world units"
 * (no fixed-point scaling needed here since coordinates are whole
 * numbers already).
 * --------------------------------------------------------------------- */

#define CUBE_SIZE 60

typedef struct { i32 x, y, z; } vec3_t;

static const vec3_t g_cube_verts[8] = {
    { -CUBE_SIZE, -CUBE_SIZE, -CUBE_SIZE },
    {  CUBE_SIZE, -CUBE_SIZE, -CUBE_SIZE },
    {  CUBE_SIZE,  CUBE_SIZE, -CUBE_SIZE },
    { -CUBE_SIZE,  CUBE_SIZE, -CUBE_SIZE },
    { -CUBE_SIZE, -CUBE_SIZE,  CUBE_SIZE },
    {  CUBE_SIZE, -CUBE_SIZE,  CUBE_SIZE },
    {  CUBE_SIZE,  CUBE_SIZE,  CUBE_SIZE },
    { -CUBE_SIZE,  CUBE_SIZE,  CUBE_SIZE },
};

static const u8 g_cube_edges[12][2] = {
    {0,1},{1,2},{2,3},{3,0},   /* back face   */
    {4,5},{5,6},{6,7},{7,4},   /* front face  */
    {0,4},{1,5},{2,6},{3,7},   /* connectors  */
};

static vec3_t rotate_xyz(vec3_t v, i32 ax, i32 ay, i32 az) {
    i32 sx = sin1000(ax), cx = cos1000(ax);
    i32 sy = sin1000(ay), cy = cos1000(ay);
    i32 sz = sin1000(az), cz = cos1000(az);

    /* rotate around X */
    i32 y1 = (v.y * cx - v.z * sx) / FP;
    i32 z1 = (v.y * sx + v.z * cx) / FP;
    i32 x1 = v.x;

    /* rotate around Y */
    i32 x2 = (x1 * cy + z1 * sy) / FP;
    i32 z2 = (-x1 * sy + z1 * cy) / FP;
    i32 y2 = y1;

    /* rotate around Z */
    i32 x3 = (x2 * cz - y2 * sz) / FP;
    i32 y3 = (x2 * sz + y2 * cz) / FP;
    i32 z3 = z2;

    vec3_t out = { x3, y3, z3 };
    return out;
}

#define CAMERA_DIST   260
#define PROJ_SCALE    240

static void project(vec3_t v, u64 screen_w, u64 screen_h, i32 *sx, i32 *sy) {
    i32 depth = v.z + CAMERA_DIST;
    if (depth < 1) depth = 1; /* avoid div-by-zero / behind-camera blowup */

    *sx = (i32)(screen_w / 2) + (v.x * PROJ_SCALE) / depth;
    *sy = (i32)(screen_h / 2) - (v.y * PROJ_SCALE) / depth;
}

static inline void plot(u64 w, u64 h, i32 x, i32 y, u32 color) {
    if (x < 0 || y < 0 || (u64)x >= w || (u64)y >= h) return;
    fb_put_pixel((u64)x, (u64)y, color);
}

/* Bresenham line, clipped per-pixel against the framebuffer bounds. */
static void draw_line(u64 w, u64 h, i32 x0, i32 y0, i32 x1, i32 y1, u32 color) {
    i32 dx = (x1 > x0) ? (x1 - x0) : (x0 - x1);
    i32 dy = (y1 > y0) ? (y0 - y1) : (y1 - y0); /* note: negated for the algorithm */
    i32 sx = (x0 < x1) ? 1 : -1;
    i32 sy = (y0 < y1) ? 1 : -1;
    i32 err = dx + dy;

    while (1) {
        plot(w, h, x0, y0, color);
        if (x0 == x1 && y0 == y1) break;

        i32 e2 = 2 * err;
        if (e2 >= dy) { err += dy; x0 += sx; }
        if (e2 <= dx) { err += dx; y0 += sy; }
    }
}

void prog_cube3d(user_t *user, const char *args) {
    (void)user;
    (void)args;

    u64 width  = fb_width();
    u64 height = fb_height();

    i32 angle_x = 0;
    i32 angle_y = 0;
    i32 angle_z = 0;

    fb_clear(0x000000);
    fb_present();

    while (1) {
#if CONFIG_KEYBOARD
        if (keyboard_has_char()) {
            keyboard_read_char();
            break;
        }
#endif

        fb_clear(0x000000);

        i32 sxs[8], sys[8];
        for (int i = 0; i < 8; i++) {
            vec3_t rotated = rotate_xyz(g_cube_verts[i], angle_x, angle_y, angle_z);
            project(rotated, width, height, &sxs[i], &sys[i]);
        }

        for (int e = 0; e < 12; e++) {
            u8 a = g_cube_edges[e][0];
            u8 b = g_cube_edges[e][1];
            draw_line(width, height, sxs[a], sys[a], sxs[b], sys[b], 0x00CFFF);
        }

        fb_present();

        angle_x = (angle_x + 2) % 360;
        angle_y = (angle_y + 3) % 360;
        angle_z = (angle_z + 1) % 360;

#if CONFIG_PIT
        pit_sleep_ms(16); /* ~60 fps */
#endif
    }

    fb_clear(0x000000);
    fb_present();
}