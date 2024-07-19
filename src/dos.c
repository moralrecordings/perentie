#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>

#include <dos.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/nearptr.h>
#include <termios.h>
#include <unistd.h>

#include "dos.h"
#include "image.h"
#include "rect.h"

typedef unsigned char byte;
typedef unsigned short word;

inline byte *vga_ptr() {
    return (byte *)0xA0000 + __djgpp_conventional_base;
}

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
   
    byte *VGA = vga_ptr();

    int16_t width = rect_width(ir);
    for (int yi = ir->top; yi < ir->bottom; ) {
        memcpy(
            (VGA + (y << 8) + (y << 6) + x),
            (image->data + (yi << image->shift) + ir->left),
            width
        );
        yi++;
        y++;
    }
    destroy_rect(ir);
    destroy_rect(crop);
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


void serial_test() {
    // Used for debug console.
    // Rely on DJGPP's POSIX compatibility layer for doing all the work.
    int port = open("COM4", O_RDWR | O_NOCTTY | O_NONBLOCK);
    if (port < 0) {
        printf("Couldn't open serial (%d): %s", errno, strerror(errno));
        return;
    }
    struct termios tty;
    if (tcgetattr(port, &tty)) {
        printf("Couldn't fetch termios settings for port (%d): %s", errno, strerror(errno));
        close(port);
        return;
    }
    // Disable parity bit
    tty.c_cflag &= ~PARENB;
    // 1 stop bit
    tty.c_cflag &= ~CSTOPB;
    // 8 bits per byte
    tty.c_cflag &= ~CSIZE;
    tty.c_cflag |= CS8;
    // 38400 baud, fastest speed known to science
    tty.c_cflag |= B38400;
    // Begone, worthless modem control lines
    tty.c_cflag |= CLOCAL;

}

