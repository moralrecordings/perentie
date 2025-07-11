#ifndef PERENTIE_SYSTEM_H
#define PERENTIE_SYSTEM_H
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>

#include "colour.h"

typedef uint8_t byte;
typedef struct pt_image pt_image;
typedef struct pt_timer_slot pt_timer_slot;

typedef struct pt_drv_app pt_drv_app;
typedef struct pt_drv_timer pt_drv_timer;
typedef struct pt_drv_keyboard pt_drv_keyboard;
typedef struct pt_drv_mouse pt_drv_mouse;
typedef struct pt_drv_serial pt_drv_serial;
typedef struct pt_drv_opl pt_drv_opl;
typedef struct pt_drv_beep pt_drv_beep;
typedef struct pt_drv_video pt_drv_video;
typedef struct pt_system pt_system;

typedef uint32_t (*pt_timer_callback)(void* data, uint32_t timer_id, uint32_t interval);

struct pt_timer_slot {
    uint32_t id;
    pt_timer_callback callback;
    void* param;
    uint32_t interval;
    uint32_t count;
};

struct pt_drv_app {
    void (*init)();
    void (*set_meta)(const char* name, const char* version, const char* identifier);
    char* (*get_data_path)();
    void (*shutdown)();
};

struct pt_drv_timer {
    void (*init)();
    void (*shutdown)();
    uint32_t (*ticks)();
    uint32_t (*millis)();
    void (*sleep)(uint32_t millis);
    uint32_t (*add_callback)(uint32_t interval, pt_timer_callback callback, void* param);
    bool (*remove_callback)(uint32_t id);
    bool (*supports_hires)();
    bool (*get_hires)();
    void (*set_hires)(bool enabled);
};

struct pt_drv_keyboard {
    void (*init)();
    void (*update)();
    void (*shutdown)();
    void (*set_key_repeat)(bool allow);
    bool (*is_key_down)(const char* key);
};

enum pt_mouse_button { MOUSE_BUTTON_LEFT, MOUSE_BUTTON_RIGHT, MOUSE_BUTTON_MIDDLE, MOUSE_BUTTON_MAX };

struct pt_drv_mouse {
    void (*init)();
    void (*update)();
    void (*shutdown)();
    int (*get_x)();
    int (*get_y)();
    bool (*is_button_down)(enum pt_mouse_button button);
    bool (*using_touch)();
};

struct pt_drv_serial {
    void (*init)();
    void (*shutdown)();
    void (*open_device)(const char* device);
    void (*close_device)();
    bool (*rx_ready)();
    bool (*tx_ready)();
    byte (*getc)();
    int (*gets)(byte* buffer, size_t length);
    void (*putc)(byte data);
    size_t (*write)(const void* buffer, size_t size);
    int (*printf)(const char* format, ...);
};

struct pt_drv_opl {
    void (*init)();
    void (*shutdown)();
    void (*write_reg)(uint16_t addr, uint8_t data);
    bool (*is_ready)();
    uint32_t (*add_callback)(uint32_t interval, pt_timer_callback callback, void* param);
    bool (*remove_callback)(uint32_t id);
};

struct pt_drv_beep {
    void (*init)();
    void (*shutdown)();
    void (*set_gate)(bool enabled);
    void (*set_mode)(uint16_t mode, bool word);
    void (*set_counter_8)(uint8_t counter);
    void (*set_counter_16)(uint16_t counter);
};

enum pt_blit_flags { FLIP_H = 0x01, FLIP_V = 0x02 };

struct pt_drv_video {
    void (*init)();
    void (*shutdown)();
    void (*clear)();
    void (*blit_image)(
        pt_image* image, int16_t x, int16_t y, uint8_t flags, int16_t left, int16_t top, int16_t right, int16_t bottom);
    void (*blit_line)(int16_t x0, int16_t y0, int16_t x1, int16_t y1, pt_colour_rgb* colour);
    void (*blit)();
    void (*flip)();
    void (*update_palette_slot)(uint8_t idx);
    void (*destroy_hw_image)(void* hw_image);
    void (*set_palette_remapper)(enum pt_palette_remapper remapper, enum pt_palette_remapper_mode mode);
    void (*set_overscan_colour)(pt_colour_rgb* colour);
    void (*get_screen_dims)(uint16_t* w, uint16_t* h);
};

struct pt_system {
    pt_drv_app* app;
    pt_drv_timer* timer;
    pt_drv_keyboard* keyboard;
    pt_drv_mouse* mouse;
    pt_drv_serial* serial;
    pt_drv_opl* opl;
    pt_drv_beep* beep;
    pt_drv_video* video;
    int palette_top;
    int palette_revision;
    pt_colour_rgb palette[256];
    enum pt_palette_remapper remapper;
    enum pt_palette_remapper_mode remapper_mode;
    uint8_t overscan;
    pt_dither dither[256];
};

extern pt_system pt_sys;

#endif
