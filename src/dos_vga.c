#include <stdint.h>
#include <string.h>

#include <bios.h>
#include <dos.h>
#include <dpmi.h>
#include <go32.h>
#include <sys/nearptr.h>

#include "colour.h"
#include "dos.h"
#include "image.h"
#include "log.h"
#include "rect.h"
#include "utils.h"

// VGA blitter

static byte vga_palette[256 * 3] = { 0 };
static pt_dither vga_dither[256] = { 0 };
static pt_colour_oklab* ega_dither_list = NULL;
static enum pt_palette_remapper vga_remapper = REMAPPER_NONE;

void set_dither_from_remapper(uint8_t idx, pt_dither* dest)
{
    switch (vga_remapper) {
    case REMAPPER_EGA:
        get_ega_dither_for_color(ega_dither_list, 256, &pt_sys.palette[idx], dest);
        break;
    case REMAPPER_NONE:
    default:
        // Don't auto change the dither.
        // Can be manually overridden.
        break;
    }
}

inline byte* vga_ptr()
{
    return (byte*)0xA0000 + __djgpp_conventional_base;
}

#define VGA_SC_INDEX 0x3c4
#define VGA_SC_DATA 0x3c5
#define VGA_GC_INDEX 0x3ce
#define VGA_GC_DATA 0x3cf
#define VGA_CRTC_INDEX 0x3d4
#define VGA_CRTC_DATA 0x3d5
#define VGA_INPUT_STATUS_1 0x3da

// clang-format off
uint8_t vga_remap[] = {
    0x00, 0x00, 0x00, 0x01, 0x01, 0x01, 0x01, 0x02,
    0x02, 0x02, 0x02, 0x03, 0x03, 0x03, 0x03, 0x04,
    0x04, 0x04, 0x04, 0x05, 0x05, 0x05, 0x05, 0x06,
    0x06, 0x06, 0x06, 0x07, 0x07, 0x07, 0x07, 0x08,
    0x08, 0x08, 0x08, 0x09, 0x09, 0x09, 0x09, 0x0a,
    0x0a, 0x0a, 0x0a, 0x0b, 0x0b, 0x0b, 0x0b, 0x0c,
    0x0c, 0x0c, 0x0c, 0x0d, 0x0d, 0x0d, 0x0d, 0x0e,
    0x0e, 0x0e, 0x0e, 0x0f, 0x0f, 0x0f, 0x0f, 0x10,
    0x10, 0x10, 0x10, 0x11, 0x11, 0x11, 0x11, 0x12,
    0x12, 0x12, 0x12, 0x13, 0x13, 0x13, 0x13, 0x14,
    0x14, 0x14, 0x14, 0x15, 0x15, 0x15, 0x15, 0x15,
    0x16, 0x16, 0x16, 0x16, 0x17, 0x17, 0x17, 0x17,
    0x18, 0x18, 0x18, 0x18, 0x19, 0x19, 0x19, 0x19,
    0x1a, 0x1a, 0x1a, 0x1a, 0x1b, 0x1b, 0x1b, 0x1b,
    0x1c, 0x1c, 0x1c, 0x1c, 0x1d, 0x1d, 0x1d, 0x1d,
    0x1e, 0x1e, 0x1e, 0x1e, 0x1f, 0x1f, 0x1f, 0x1f,
    0x20, 0x20, 0x20, 0x20, 0x21, 0x21, 0x21, 0x21,
    0x22, 0x22, 0x22, 0x22, 0x23, 0x23, 0x23, 0x23,
    0x24, 0x24, 0x24, 0x24, 0x25, 0x25, 0x25, 0x25,
    0x26, 0x26, 0x26, 0x26, 0x27, 0x27, 0x27, 0x27,
    0x28, 0x28, 0x28, 0x28, 0x29, 0x29, 0x29, 0x29,
    0x2a, 0x2a, 0x2a, 0x2a, 0x2a, 0x2b, 0x2b, 0x2b,
    0x2b, 0x2c, 0x2c, 0x2c, 0x2c, 0x2d, 0x2d, 0x2d,
    0x2d, 0x2e, 0x2e, 0x2e, 0x2e, 0x2f, 0x2f, 0x2f,
    0x2f, 0x30, 0x30, 0x30, 0x30, 0x31, 0x31, 0x31,
    0x31, 0x32, 0x32, 0x32, 0x32, 0x33, 0x33, 0x33,
    0x33, 0x34, 0x34, 0x34, 0x34, 0x35, 0x35, 0x35,
    0x35, 0x36, 0x36, 0x36, 0x36, 0x37, 0x37, 0x37,
    0x37, 0x38, 0x38, 0x38, 0x38, 0x39, 0x39, 0x39,
    0x39, 0x3a, 0x3a, 0x3a, 0x3a, 0x3b, 0x3b, 0x3b,
    0x3b, 0x3c, 0x3c, 0x3c, 0x3c, 0x3d, 0x3d, 0x3d,
    0x3d, 0x3e, 0x3e, 0x3e, 0x3e, 0x3f, 0x3f, 0x3f
};

// clang-format on
static byte* vga_framebuffer = NULL;

static bool vga_available = false;

static int vga_page_offset = 0;

static int vga_revision = 0;

void vga_shutdown();

void vga_init()
{
    // Memory protection is for chumps
    if (__djgpp_nearptr_enable() == 0) {
        log_print("Couldn't access the first 640K of memory. Boourns.\n");
        exit(-1);
    }
    union REGS regs;
    // INT 10h AX=1a00h - Get display combination code
    regs.h.ah = 0x1a;
    regs.h.al = 0x00;
    regs.h.bh = 0x00;
    regs.h.bl = 0x00;
    int86(0x10, &regs, &regs);
    if (regs.h.bl == 0x00) {
        log_print("VGA not found, aborting");
        return;
    }

    // Mode 13h - raster, 256 colours, 320x200
    regs.h.ah = 0x00;
    regs.h.al = 0x13;
    int86(0x10, &regs, &regs);

    // Use "Mode X" - two whole frames in VGA memory,
    // flipping between them.
    // The downside is that memory is no longer linear,
    // instead we have 4 planes.

    // Turn off chain-4 mode
    outportb(VGA_SC_INDEX, 0x4);
    outportb(VGA_SC_DATA, 0x06);

    // Set map mask to all 4 planes
    outportb(VGA_SC_INDEX, 0x02);
    outportb(VGA_SC_DATA, 0x0f);
    // Clear all 256kb of VGA memory
    memset(vga_ptr(), 0, 0x10000);
    // Set map mask to plane 1
    outportb(VGA_SC_INDEX, 0x02);
    outportb(VGA_SC_DATA, 0x01);
    // Set read plane to 1
    outportb(VGA_GC_INDEX, 0x03);
    outportb(VGA_GC_DATA, 0x00);

    // Underline location register - Turn off long mode
    outportb(VGA_CRTC_INDEX, 0x14);
    outportb(VGA_CRTC_DATA, 0x00);

    // Mode control register - Turn on byte mode
    outportb(VGA_CRTC_INDEX, 0x17);
    outportb(VGA_CRTC_DATA, 0xe3);

    vga_framebuffer = (byte*)calloc(SCREEN_WIDTH * SCREEN_HEIGHT, sizeof(byte));

    for (int i = 0; i < pt_sys.palette_top; i++) {
        vga_palette[3 * i] = vga_remap[pt_sys.palette[i].r];
        vga_palette[3 * i + 1] = vga_remap[pt_sys.palette[i].g];
        vga_palette[3 * i + 2] = vga_remap[pt_sys.palette[i].b];
    }

    // Generate the EGA dither table.
    // Move this to the EGA driver later on.

    ega_dither_list = generate_ega_dither_list();

    atexit(vga_shutdown);
    vga_available = true;
}

void vga_clear()
{
    if (!vga_framebuffer)
        return;
    memset(vga_framebuffer, 0, SCREEN_WIDTH * SCREEN_HEIGHT);
}

pt_image_vga* vga_convert_image(pt_image* image);
void vga_destroy_hw_image(void* hw_image);

void vga_blit_image(
    pt_image* image, int16_t x, int16_t y, uint8_t flags, int16_t left, int16_t top, int16_t right, int16_t bottom)
{
    if (!vga_framebuffer) {
        log_print("vga_blit_image: driver not inited!\n");
        return;
    }
    if (!image) {
        log_print("vga_blit_image: image is NULL!\n");
        return;
    }

    if (image->hw_image && ((pt_image_vga*)image->hw_image)->revision != vga_revision) {
        vga_destroy_hw_image(image->hw_image);
        image->hw_image = NULL;
    }

    if (!image->hw_image) {
        image->hw_image = (void*)vga_convert_image(image);
    }

    pt_image_vga* hw_image = (pt_image_vga*)image->hw_image;

    struct rect ir = { MIN(image->width, MAX(0, left)), MIN(image->height, MAX(0, top)),
        MIN(image->width, MAX(0, right)), MIN(image->height, MAX(0, bottom)) };

    struct rect crop = { 0, 0, SCREEN_WIDTH, SCREEN_HEIGHT };
    // log_print("Blitting %s to (%d,%d) %dx%d ->", image->path, x, y, image->width, image->height);

    // Constrain x and y to be an absolute start offset in screen space.
    // Crop the image rectangle, based on its location in screen space.
    if (!rect_blit_clip(&x, &y, &ir, &crop)) {
        // log_print("Rectangle off screen\n");
        return;
    }

    // after the image rect has been clipped, flip it if required
    if (flags & FLIP_H) {
        int16_t tmp = ir.right;
        ir.right = hw_image->width - ir.left;
        ir.left = hw_image->width - tmp;
    }

    if (flags & FLIP_V) {
        int16_t tmp = ir.bottom;
        ir.bottom = hw_image->height - ir.top;
        ir.top = hw_image->height - tmp;
    }

    // log_print("image coords for %s: (%d, %d, %d, %d) %dx%d -> (%d, %d)\n", image->path, ir.left, ir.top, ir.right,
    // ir.bottom, rect_width(&ir), rect_height(&ir), x, y);

    // The source data is then drawn from the image rectangle.

    // y start position in the framebuffer
    int16_t y_start = y;
    // plane ID in the source image.
    for (int pi = 0; pi < 4; pi++) {
        // destination plane ID in the framebuffer
        // don't ask me how the below works!
        // it was found by trial and error to sort the planes properly!
        uint8_t pf = (flags & FLIP_H) ? ((x + 3 + (((ir.right) % 4)) - pi) % 4) : ((x + (4 - (ir.left % 4)) + pi) % 4);
        // image x start/end positions (corrected for plane)
        int16_t ir_left = (ir.left >> 2) + ((ir.left % 4) > pi ? 1 : 0);
        int16_t ir_right = (ir.right >> 2) + ((ir.right % 4) > pi ? 1 : 0);
        // adjust ir_right - ir_left to be a multiple of 4, then move any overflow onto ir_xtra
        int16_t ir_xtra = (ir_right - ir_left) % 4;
        ir_right -= ir_xtra;
        // log_print("plane: %d -> %d, ir_left: %d -> %d, ir_right: %d -> %d\n", pi, pf, ir.left, ir_left, ir.right,
        // ir_right);
        //   framebuffer x start position (corrected for plane)
        int16_t fx = (x >> 2) + ((x % 4) > pf ? 1 : 0);

        // log_print("pi: %d, pf: %d, ir_left: %d, ir_right: %d, fx: %d\n", pi, pf, ir_left, ir_right, fx);
        // start address of the current plane of the source image
        uint8_t* hw_bitmap_base = hw_image->bitmap + pi * hw_image->plane;
        // start address of the current plane of the source image mask
        uint8_t* hw_mask_base = hw_image->mask + pi * hw_image->plane;
        // start address of the current plane of the framebuffer
        uint8_t* fb_base = vga_framebuffer + pf * SCREEN_PLANE;
        // log_print("hw_base: %d, fb_base: %d\n", pi * hw_image->plane, pf * SCREEN_PLANE);

        for (int yi = ir.top; yi < ir.bottom; yi++) {
            // start address of the current horizontal run of pixels in the source image
            uint8_t* hw_bitmap = hw_bitmap_base + yi * hw_image->plane_pitch + ir_left;
            // start address of the current horizontal run of pixels in the source image mask
            uint8_t* hw_mask = hw_mask_base + yi * hw_image->plane_pitch + ir_left;
            // invert framebuffer y coordinate if vertical flipped
            int16_t yf = (flags & FLIP_V) ? ((ir.bottom - ir.top - 1) - (y - y_start) + y_start) : y;
            // start address of the current horizontal run of pixels in the framebuffer
            uint8_t* fb_ptr = fb_base + (yf * (SCREEN_WIDTH >> 2)) + fx;
            if (flags & FLIP_H) {
                // invert framebuffer x position if horizontal flipped
                fb_ptr += ir_xtra + ir_right - ir_left - 1;
                for (int i = 0; i < ir_xtra; i++) {
                    *fb_ptr = (*fb_ptr & ~(*hw_mask)) | (*hw_bitmap & *hw_mask);
                    hw_bitmap++;
                    hw_mask++;
                    fb_ptr--;
                }
                for (int xi = ir_left; xi < ir_right; xi += 1) {
                    // log_print("xi: %d, yi: %d, pi: %d, hw_off: %d, fb_off: %d\n", xi, yi, pi, hw_bitmap -
                    //         hw_image->bitmap, fb_ptr - vga_framebuffer);
                    //  in the framebuffer, replace masked bits with source image data
                    *fb_ptr = (*fb_ptr & ~(*hw_mask)) | (*hw_bitmap & *hw_mask);
                    hw_bitmap++;
                    hw_mask++;
                    fb_ptr--;
                }
            } else {
                for (int xi = ir_left; xi < ir_right; xi += 4) {
                    // log_print("xi: %d, yi: %d, pi: %d, hw_off: %d, fb_off: %d\n", xi, yi, pi, hw_bitmap -
                    // hw_image->bitmap, fb_ptr - vga_framebuffer);
                    // in the framebuffer, replace masked bits with source image data
                    *(uint32_t*)fb_ptr
                        = (*(uint32_t*)fb_ptr & ~(*(uint32_t*)hw_mask)) | (*(uint32_t*)hw_bitmap & *(uint32_t*)hw_mask);
                    hw_bitmap += 4;
                    hw_mask += 4;
                    fb_ptr += 4;
                }
                for (int i = 0; i < ir_xtra; i++) {
                    *fb_ptr = (*fb_ptr & ~(*hw_mask)) | (*hw_bitmap & *hw_mask);
                    hw_bitmap++;
                    hw_mask++;
                    fb_ptr++;
                }
            }
            y++;
        }
        y = y_start;
    }
}

void vga_load_palette_colour(int idx)
{
    disable();
    outportb(0x3c6, 0xff);
    outportb(0x3c8, (uint8_t)idx);
    outportb(0x3c9, vga_palette[3 * idx]);
    outportb(0x3c9, vga_palette[3 * idx + 1]);
    outportb(0x3c9, vga_palette[3 * idx + 2]);
    enable();
}

uint8_t vga_map_colour(uint8_t r, uint8_t g, uint8_t b)
{
    for (int i = 0; i < pt_sys.palette_top; i++) {
        if (pt_sys.palette[i].r == r && pt_sys.palette[i].g == g && pt_sys.palette[i].b == b)
            return i;
    }
    byte r_v = vga_remap[r];
    byte g_v = vga_remap[g];
    byte b_v = vga_remap[b];
    // add a new colour
    if (pt_sys.palette_top < 256) {
        int idx = pt_sys.palette_top;
        pt_sys.palette_top++;
        pt_sys.palette[idx].r = r;
        pt_sys.palette[idx].g = g;
        pt_sys.palette[idx].b = b;
        vga_palette[3 * idx] = r_v;
        vga_palette[3 * idx + 1] = g_v;
        vga_palette[3 * idx + 2] = b_v;
        vga_load_palette_colour(idx);
        set_dither_from_remapper(idx, &vga_dither[idx]);
        log_print("vga_map_colour: vga_palette[%d] = %d, %d, %d -> %d, %d, %d (dither %d %d)\n", idx, r, g, b, r_v, g_v,
            b_v, vga_dither[idx].idx_a, vga_dither[idx].idx_b);
        vga_revision++;
        return idx;
    }
    // Out of palette slots; need to macguyver the nearest colour.
    // Formula borrowed from ScummVM's palette code.
    uint8_t best_color = 0;
    uint32_t min = 0xffffffff;
    for (int i = 0; i < 256; ++i) {
        int rmean = (vga_palette[3 * i + 0] + r_v) / 2;
        int dr = vga_palette[3 * i + 0] - r_v;
        int dg = vga_palette[3 * i + 1] - g_v;
        int db = vga_palette[3 * i + 2] - b_v;

        uint32_t dist_squared = (((512 + rmean) * dr * dr) >> 8) + 4 * dg * dg + (((767 - rmean) * db * db) >> 8);
        if (dist_squared < min) {
            best_color = i;
            min = dist_squared;
        }
    }
    return best_color;
}

void vga_plot(int16_t x, int16_t y, uint8_t value)
{
    if ((x < 0) || (x >= SCREEN_WIDTH) || (y < 0) || (y >= SCREEN_HEIGHT))
        return;
    if (!vga_framebuffer)
        return;
    *(vga_framebuffer + ((x % 4) * SCREEN_PLANE) + (y * (SCREEN_WIDTH >> 2)) + (x >> 2)) = value;
}

void vga_blit_line(int16_t x0, int16_t y0, int16_t x1, int16_t y1, pt_colour_rgb* colour)
{
    // bresenham line algorithm - borrowed from ScummVM's drawLine function
    const bool steep = abs(y1 - y0) > abs(x1 - x0);

    if (steep) {
        int16_t tmp;
        tmp = x0;
        x0 = y0;
        y0 = tmp;
        tmp = x1;
        x1 = y1;
        y1 = tmp;
    }

    int16_t dx = abs(x1 - x0);
    int16_t dy = abs(y1 - y0);
    int16_t D = dy;
    int16_t x = x0;
    int16_t y = y0;
    int16_t err = 0;
    int16_t x_inc = (x0 < x1) ? 1 : -1;
    int16_t y_inc = (y0 < y1) ? 1 : -1;

    uint8_t value = vga_map_colour(colour->r, colour->g, colour->b);

    if (steep)
        vga_plot(y, x, value);
    else
        vga_plot(x, y, value);

    while (x != x1) {
        x += x_inc;
        err += D;
        if (2 * err > dx) {
            y += y_inc;
            err -= dx;
        }
        if (steep)
            vga_plot(y, x, value);
        else
            vga_plot(x, y, value);
    }
}

bool vga_is_vblank()
{
    // sleep until the start of the next vertical blanking interval
    // - do drawing cycle
    // - wait until bit is 0
    // - flip page
    // - wait until bit is 1

    // CRT mode and status - CGA/EGA/VGA input status 1 register - in vertical retrace
    return inportb(VGA_INPUT_STATUS_1) & 8;
}

bool vga_is_not_drawing()
{
    // CRT mode and status = CGA/EGA/VGA input status 1 register - display disabled
    return inportb(VGA_INPUT_STATUS_1) & 1;
}

void vga_blit()
{
    // copy the framebuffer to VGA memory
    if (!vga_framebuffer)
        return;
    byte* vga = vga_ptr() + vga_page_offset;
    byte* buf = vga_framebuffer;
    // Set map mask to plane 0
    outportb(VGA_SC_INDEX, 0x02);
    outportb(VGA_SC_DATA, 0x01);
    memcpy(vga, buf, SCREEN_PLANE);
    // Set map mask to plane 1
    outportb(VGA_SC_DATA, 0x02);
    buf += SCREEN_PLANE;
    memcpy(vga, buf, SCREEN_PLANE);
    // Set map mask to plane 2
    outportb(VGA_SC_DATA, 0x04);
    buf += SCREEN_PLANE;
    memcpy(vga, buf, SCREEN_PLANE);
    // Set map mask to plane 3
    outportb(VGA_SC_DATA, 0x08);
    buf += SCREEN_PLANE;
    memcpy(vga, buf, SCREEN_PLANE);
}

static int frame_count = 0;

void vga_flip()
{
    if (!vga_framebuffer)
        return;

    uint32_t ticks = pt_sys.timer->ticks();
    // Do not yield in the busy-loop.
    // Yielding here causes massive lag under Windows 98,
    // and no other games seem to do it.

    // Confirm that the screen is in the middle of being drawn.
    // Why? Well, the start address gets latched exactly once,
    // at the start of the vblank interval.
    // Which means as soon as we detect a vblank it's too late,
    // the screen will tear.

    // On testing on real hardware, this causes a lot more frameskips.
    // Don't bother.
    // do {
    //} while (vga_is_not_drawing());
    // uint32_t draw_wait = pt_sys.timer->ticks() - ticks;

    // Flip page by setting the CRTC data address to the
    // area of VGA memory we just wrote to with vga_blit()
    // ticks = pt_sys.timer->ticks();
    disable();
    outportb(VGA_CRTC_INDEX, 0x0c);
    outportb(VGA_CRTC_DATA, 0xff & (vga_page_offset >> 8));
    outportb(VGA_CRTC_INDEX, 0x0d);
    outportb(VGA_CRTC_DATA, 0xff & vga_page_offset);
    enable();
    // uint32_t flip_wait = pt_sys.timer->ticks() - ticks;

    // Swap the VGA offset used for writes to the other
    // region of memory
    vga_page_offset = (vga_page_offset > 0) ? 0 : SCREEN_PLANE;

    // Wait until the start of the vertical blanking interval
    // ticks = pt_sys.timer->ticks();
    do {
    } while (!vga_is_vblank());
    // uint32_t vblankstart = pt_sys.timer->ticks() - ticks;

    // Wait until vblank is over
    // ticks = pt_sys.timer->ticks();
    do {
    } while (vga_is_vblank());
    // uint32_t vblankend = pt_sys.timer->ticks() - ticks;

    // A little yield as a treat
    sys_yield();

    frame_count += 1;
    // if ((frame_count % 100) == 0)
    //     log_print("vga_blit: draw %d flip %d vblankstart %d vblankend %d\n", draw_wait, flip_wait, vblankstart,
    //     vblankend);
}

pt_image_vga* vga_convert_image(pt_image* image)
{
    pt_image_vga* result = (pt_image_vga*)calloc(1, sizeof(pt_image_vga));
    result->width = image->width;
    result->height = image->height;
    result->pitch = image->pitch;
    result->plane = (image->pitch * image->height) >> 2;
    result->plane_pitch = (image->pitch) >> 2;
    result->bitmap = (byte*)calloc(result->pitch * result->height, sizeof(byte));
    result->mask = (byte*)calloc(result->pitch * result->height, sizeof(byte));
    result->revision = vga_revision;

    byte palette_map[256];
    for (int i = 0; i < 256; i++) {
        palette_map[i] = vga_map_colour(image->palette[3 * i], image->palette[3 * i + 1], image->palette[3 * i + 2]);
    }

    for (int p = 0; p < 4; p++) {
        byte* bitmap = result->bitmap + (p * result->plane);
        byte* mask = result->mask + (p * result->plane);
        for (int y = 0; y < result->height; y++) {
            for (int x = 0; x < result->plane_pitch; x++) {
                byte pixel = image->data[y * result->pitch + (x << 2) + p];
                pt_dither* dither = &vga_dither[palette_map[pixel]];
                switch (dither->type) {
                case DITHER_D50:
                    bitmap[y * result->plane_pitch + x] = (((x << 2) + y + p) % 2) ? dither->idx_b : dither->idx_a;
                    break;
                case DITHER_FILL_A:
                    bitmap[y * result->plane_pitch + x] = dither->idx_a;
                    break;
                case DITHER_FILL_B:
                    bitmap[y * result->plane_pitch + x] = dither->idx_b;
                    break;
                case DITHER_NONE:
                default:
                    bitmap[y * result->plane_pitch + x] = palette_map[pixel];
                    break;
                }
                mask[y * result->plane_pitch + x]
                    = ((pixel == image->colourkey) || (image->palette_alpha[pixel] == 0x00)) ? 0x00 : 0xff;
            }
        }
    }

    return result;
}

void vga_destroy_hw_image(void* hw_image)
{
    pt_image_vga* image = (pt_image_vga*)hw_image;
    if (!image)
        return;
    if (image->bitmap) {
        free(image->bitmap);
        image->bitmap = NULL;
    }
    if (image->mask) {
        free(image->mask);
        image->mask = NULL;
    }
    free(image);
}

void vga_set_palette_remapper(enum pt_palette_remapper remapper)
{
    vga_remapper = remapper;
    for (int i = 0; i < pt_sys.palette_top; i++) {
        if (remapper == REMAPPER_NONE) {
            // If we're explicitly setting the mapper to be NONE,
            // clear the dither table.
            vga_dither[i].type = DITHER_NONE;
            vga_dither[i].idx_a = 0;
            vga_dither[i].idx_b = 0;
        } else {
            set_dither_from_remapper(i, &vga_dither[i]);
        }
    }

    // Force all hardware images to be recalculated
    vga_revision++;
}

void vga_set_dither_hint(pt_colour_rgb* src, enum pt_dither_type type, pt_colour_rgb* a, pt_colour_rgb* b)
{
    uint8_t idx_src = vga_map_colour(src->r, src->g, src->b);
    uint8_t idx_a = vga_map_colour(a->r, a->g, a->b);
    uint8_t idx_b = vga_map_colour(b->r, b->g, b->b);
    vga_dither[idx_src].type = type;
    vga_dither[idx_src].idx_a = idx_a;
    vga_dither[idx_src].idx_b = idx_b;
    vga_revision++;
}

void vga_shutdown()
{
    // Mode 3h - text, 16 colours, 80x25
    union REGS regs;
    regs.h.ah = 0x00;
    regs.h.al = 0x03;
    int86(0x10, &regs, &regs);
    // Fine, maybe we should stop being unsafe
    __djgpp_nearptr_disable();
    if (vga_framebuffer) {
        free(vga_framebuffer);
        vga_framebuffer = NULL;
    }
    if (ega_dither_list) {
        free(ega_dither_list);
        ega_dither_list = NULL;
    }
}

pt_drv_video dos_vga = { &vga_init, &vga_shutdown, &vga_clear, &vga_blit_image, &vga_blit_line, &vga_is_vblank,
    &vga_blit, &vga_flip, &vga_map_colour, &vga_destroy_hw_image, &vga_set_palette_remapper, &vga_set_dither_hint };
