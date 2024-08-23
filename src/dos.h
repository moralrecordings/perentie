#ifndef PERENTIE_DOS_H
#define PERENTIE_DOS_H

#include <stdbool.h>
#include <stdint.h>

#include "system.h"

#define SCREEN_WIDTH 320
#define SCREEN_HEIGHT 200

typedef struct pt_image pt_image;
typedef struct pt_image_vga pt_image_vga;

struct pt_image_vga {
    byte* bitmap;
    byte* mask;
    uint16_t width;
    uint16_t height;
    uint16_t pitch;
};

bool sys_idle(int (*idle_callback)(), int idle_callback_period);

extern pt_drv_video dos_vga;

extern pt_drv_timer dos_timer;

extern pt_drv_beep dos_beep;

extern pt_drv_mouse dos_mouse;

extern pt_drv_keyboard dos_keyboard;

extern pt_drv_serial dos_serial;

extern pt_drv_opl dos_opl;

#endif
