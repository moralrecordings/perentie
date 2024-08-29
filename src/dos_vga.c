#include <stdint.h>
#include <string.h>

#include <bios.h>
#include <dos.h>
#include <dpmi.h>
#include <go32.h>
#include <sys/nearptr.h>

#include "dos.h"
#include "image.h"
#include "log.h"
#include "rect.h"

// VGA blitter

// clang-format off
static byte vga_palette[256*3] = {
    0, 0, 0, 
    0, 0, 42,
    0, 42, 0,
    0, 42, 42,
    42, 0, 0,
    42, 0, 42,
    42, 21, 0,
    42, 42, 42,
    21, 21, 21,
    21, 21, 63,
    21, 63, 21,
    21, 63, 63,
    63, 21, 21,
    63, 21, 63,
    63, 63, 21,
    63, 63, 63,
};
// clang-format on
static int vga_palette_top = 16;

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

static byte* vga_framebuffer = NULL;

static bool vga_available = false;

static int vga_page_offset = 0;

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

void vga_blit_image(pt_image* image, int16_t x, int16_t y)
{
    if (!vga_framebuffer) {
        log_print("vga_framebuffer missing ya dingus!\n");
        return;
    }
    if (!image) {
        log_print("WARNING: Tried to blit nothing ya dingus\n");
        return;
    }

    if (!image->hw_image) {
        image->hw_image = (void*)vga_convert_image(image);
    }

    pt_image_vga* hw_image = (pt_image_vga*)image->hw_image;

    struct rect* ir = create_rect_dims(hw_image->width, hw_image->height);

    struct rect* crop = create_rect_dims(SCREEN_WIDTH, SCREEN_HEIGHT);
    x -= image->origin_x;
    y -= image->origin_y;
    // log_print("Blitting %s to (%d,%d) %dx%d ->", image->path, x, y, image->width, image->height);

    // Constrain x and y to be an absolute start offset in screen space.
    // Crop the image rectangle, based on its location in screen space.
    if (!rect_blit_clip(&x, &y, ir, crop)) {
        // log_print("Rectangle off screen\n");
        destroy_rect(ir);
        destroy_rect(crop);
        return;
    }
    // log_print("image coords for %s: (%d, %d) %dx%d -> (%d, %d)\n", image->path, ir->left, ir->top, rect_width(ir),
    // rect_height(ir), x, y);

    // The source data is then drawn from the image rectangle.

    // y start position in the framebuffer
    int16_t y_start = y;
    // plane ID in the source image.
    for (int pi = 0; pi < 4; pi++) {
        // destination plane ID in the framebuffer
        // don't ask me how the below works!
        // it was found by trial and error to sort the planes properly!
        uint8_t pf = (x + (4 - (ir->left % 4)) + pi) % 4;
        // image x start/end positions (corrected for plane)
        int16_t ir_left = (ir->left >> 2) + ((ir->left % 4) > pi ? 1 : 0);
        int16_t ir_right = (ir->right >> 2) + ((ir->right % 4) > pi ? 1 : 0);
        // framebuffer x start position (corrected for plane)
        int16_t fx = (x >> 2) + ((x % 4) > pf ? 1 : 0);

        // log_print("pi: %d, pf: %d, ir_left: %d, ir_right: %d, fx: %d\n", pi, pf, ir_left, ir_right, fx);
        // start address of the current plane of the source image
        uint8_t* hw_bitmap_base = hw_image->bitmap + pi * hw_image->plane;
        // start address of the current plane of the source image mask
        uint8_t* hw_mask_base = hw_image->mask + pi * hw_image->plane;
        // start address of the current plane of the framebuffer
        uint8_t* fb_base = vga_framebuffer + pf * SCREEN_PLANE;
        // log_print("hw_base: %d, fb_base: %d\n", pi * hw_image->plane, pf * SCREEN_PLANE);

        for (int yi = ir->top; yi < ir->bottom; yi++) {
            // start address of the current horizontal run of pixels in the source image
            uint8_t* hw_bitmap = hw_bitmap_base + yi * hw_image->plane_pitch + ir_left;
            // start address of the current horizontal run of pixels in the source image mask
            uint8_t* hw_mask = hw_mask_base + yi * hw_image->plane_pitch + ir_left;
            // start address of the current horizontal run of pixels in the framebuffer
            uint8_t* fb_ptr = fb_base + (y * (SCREEN_WIDTH >> 2)) + fx;
            for (int xi = ir_left; xi < ir_right; xi++) {
                // log_print("xi: %d, yi: %d, pi: %d, hw_off: %d, fb_off: %d\n", xi, yi, pi, hw_bitmap -
                // hw_image->bitmap, fb_ptr - vga_framebuffer);
                //  in the framebuffer, replace masked bits with source image data
                *fb_ptr = (*fb_ptr & ~(*hw_mask)) | (*hw_bitmap & *hw_mask);
                hw_bitmap++;
                hw_mask++;
                fb_ptr++;
            }
            y++;
        }
        y = y_start;
    }
    destroy_rect(ir);
    destroy_rect(crop);
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

void vga_flip()
{
    // flip the VGA page

    if (!vga_framebuffer)
        return;

    // wait until we're out of vblank
    while (vga_is_vblank()) {
        __dpmi_yield();
    }

    // Flip page by setting the address
    disable();
    outportb(VGA_CRTC_INDEX, 0x0c);
    outportb(VGA_CRTC_DATA, 0xff & (vga_page_offset >> 8));
    outportb(VGA_CRTC_INDEX, 0x0d);
    outportb(VGA_CRTC_DATA, 0xff & vga_page_offset);
    enable();

    vga_page_offset = vga_page_offset ? 0 : SCREEN_PLANE;

    // wait until the next vblank interval starts
    while (!vga_is_vblank()) {
        __dpmi_yield();
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

uint8_t vga_map_colour(uint8_t r, uint8_t g, uint8_t b)
{
    byte r_v = vga_remap[r];
    byte g_v = vga_remap[g];
    byte b_v = vga_remap[b];
    for (int i = 0; i < vga_palette_top; i++) {
        if (vga_palette[3 * i] == r_v && vga_palette[3 * i + 1] == g_v && vga_palette[3 * i + 2] == b_v)
            return i;
    }
    // add a new colour
    if (vga_palette_top < 256) {
        int idx = vga_palette_top;
        vga_palette_top++;
        vga_palette[3 * idx] = r_v;
        vga_palette[3 * idx + 1] = g_v;
        vga_palette[3 * idx + 2] = b_v;
        vga_load_palette_colour(idx);
        log_print("vga_map_colour: vga_palette[%d] = %d, %d, %d -> %d, %d, %d\n", idx, r, g, b, r_v, g_v, b_v);
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
                bitmap[y * result->plane_pitch + x] = palette_map[pixel];
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

void vga_shutdown()
{
    // Mode 3h - text, 16 colours, 80x25
    union REGS regs;
    regs.h.ah = 0x00;
    regs.h.al = 0x03;
    int86(0x10, &regs, &regs);
    // Fine, maybe we should stop being unsafe
    __djgpp_nearptr_disable();
    free(vga_framebuffer);
    vga_framebuffer = NULL;
}

pt_drv_video dos_vga = { &vga_init, &vga_shutdown, &vga_clear, &vga_blit_image, &vga_is_vblank, &vga_blit, &vga_flip,
    &vga_map_colour, &vga_destroy_hw_image };
