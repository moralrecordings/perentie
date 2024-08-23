#ifndef PERENTIE_SYSTEM_H
#define PERENTIE_SYSTEM_H
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>

typedef uint8_t byte;
typedef struct pt_image pt_image;

typedef struct pt_drv_timer pt_drv_timer;
typedef struct pt_drv_keyboard pt_drv_keyboard;
typedef struct pt_drv_mouse pt_drv_mouse;
typedef struct pt_drv_serial pt_drv_serial;
typedef struct pt_drv_opl pt_drv_opl;
typedef struct pt_drv_beep pt_drv_beep;
typedef struct pt_drv_video pt_drv_video;
typedef struct pt_system pt_system;

struct pt_drv_timer {
    void (*init)();
    void (*shutdown)();
    uint32_t (*ticks)();
    uint32_t (*millis)();
    void (*sleep)(uint32_t);
};

struct pt_drv_keyboard {
    void (*init)();
    void (*update)();
    void (*shutdown)();
    void (*set_key_repeat)(bool);
    bool (*is_key_down)(const char*);
};

enum pt_mouse_button { MOUSE_BUTTON_LEFT, MOUSE_BUTTON_RIGHT, MOUSE_BUTTON_MIDDLE, MOUSE_BUTTON_MAX };

struct pt_drv_mouse {
    void (*init)();
    void (*update)();
    void (*shutdown)();
    int (*get_x)();
    int (*get_y)();
    bool (*is_button_down)(enum pt_mouse_button);
};

struct pt_drv_serial {
    void (*init)();
    void (*shutdown)();
    bool (*rx_ready)();
    bool (*tx_ready)();
    byte (*getc)();
    int (*gets)(byte*, size_t);
    void (*putc)(byte);
    size_t (*write)(const void*, size_t);
    int (*printf)(const char*, ...);
};

struct pt_drv_opl {
    void (*init)();
    void (*shutdown)();
    void (*write_reg)(uint16_t, uint8_t);
};

struct pt_drv_beep {
    void (*init)();
    void (*shutdown)();
    void (*tone)(float freq);
    void (*stop)();
};

struct pt_drv_video {
    void (*init)();
    void (*shutdown)();
    void (*clear)();
    void (*blit_image)(pt_image* image, int16_t x, int16_t y);
    bool (*is_vblank)();
    void (*flip)();
    uint8_t (*map_colour)(uint8_t r, uint8_t g, uint8_t b);
    void (*destroy_hw_image)(void*);
};

struct pt_system {
    pt_drv_timer* timer;
    pt_drv_keyboard* keyboard;
    pt_drv_mouse* mouse;
    pt_drv_serial* serial;
    pt_drv_opl* opl;
    pt_drv_beep* beep;
    pt_drv_video* video;
};

extern pt_system pt_sys;

#endif
