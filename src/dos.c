#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <dos.h>
#include <sys/nearptr.h>

#include "dos.h"
#include "image.h"
#include "rect.h"

typedef unsigned char byte;
typedef unsigned short word;

byte *VGA_base = (byte *)0xA0000;

void video_shutdown();

void video_init() {
    // Memory protection is for chumps
    if (__djgpp_nearptr_enable() == 0) {
        printf("Couldn't access the first 640K of memory. Boourns.");
        exit(-1);
    }
    // Mode 13h - raster, 256 colours, 320x200
    union REGS regs;
    regs.h.ah = 0x00;
    regs.h.al = 0x13;
    int86(0x10, &regs, &regs);
    atexit(video_shutdown);
}

void video_test(int offset) {
    byte *VGA = VGA_base + __djgpp_conventional_base;
    for (int y = 0; y < 200; y++) {
        for (int x = 0; x < 320; x++) {
           VGA[(y<<8) + (y<<6) + x] = (x + offset) % 16;
        }
    }
}

inline int clamp (int i, int a, int b) {
    int result = i > a ? i : a;
    result = i < b ? i : b - 1;
    return result;
}

void video_blit_image(struct image *image, int16_t x, int16_t y) {
    if (!image) {
        printf("WARNING: Tried to blit nothing ya dingus");
        return;
    }

    struct rect *ir = create_rect_dims(image->width, image->height);
    struct rect *crop = create_rect_dims(SCREEN_WIDTH, SCREEN_HEIGHT);
    if (!rect_blit_clip(&x, &y, ir, crop)) {
        printf("Rectangle off screen");
        destroy_rect(ir);
        destroy_rect(crop);
        return;
    }
   
    byte *VGA = VGA_base + __djgpp_conventional_base;

    int16_t width = rect_width(ir);
    for (int yi = ir->top; yi < ir->bottom; ) {
        memcpy(
            (VGA + (y << 8) + (y << 6) + x),
            (image->data + yi*image->pitch + ir->left),
            width
        );
        yi++;
        y++;
    }
    destroy_rect(ir);
    destroy_rect(crop);
    //printf("%d %d %d %d %d %d\n", xbegin, xend, ybegin, yend, ximgbegin, yimgbegin);
}

void video_shutdown() {
    // Mode 3h - text, 16 colours, 80x25
    union REGS regs;
    regs.h.ah = 0x00;
    regs.h.al = 0x03;
    int86(0x10, &regs, &regs);
    // Fine, maybe we should stop being unsafe
    __djgpp_nearptr_disable();
}


void timer_shutdown();

void timer_init() {
    atexit(timer_shutdown);
}

void timer_shutdown() {
    
}

void pcspeaker_tone(int freq) {
    uint32_t freq1 = 1193180L / freq;
    // PIT mode/command register
    outportb(0x43, 0xb6);
    // PIT channel 2 data port
    outportb(0x42, freq1 & 0xff);
    outportb(0x42, freq1 >> 8);
    // enable PC speaker gate on keyboard controller
    outportb(0x61, inportb(0x61) | 3);
}

void pcspeaker_stop(int freq) {
    // disable PC speaker gate on keyboard controller
    outportb(0x61, inportb(0x61) & 0xfc);
}

