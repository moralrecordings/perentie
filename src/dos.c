#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <dos.h>
#include <sys/nearptr.h>

typedef unsigned char byte;
typedef unsigned short word;

byte *VGA = (byte *)0xA0000;

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
    VGA += __djgpp_conventional_base;
    atexit(video_shutdown);
}

void video_test(int offset) {
    for (int y = 0; y < 200; y++) {
        for (int x = 0; x < 320; x++) {
           VGA[(y<<8) + (y<<6) + x] = (x + offset) % 16;
        }
    }
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
