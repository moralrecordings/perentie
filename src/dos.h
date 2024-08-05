#ifndef PERENTIE_DOS_H
#define PERENTIE_DOS_H

#include <stdbool.h>
#include <stdint.h>
#define SCREEN_WIDTH 320
#define SCREEN_HEIGHT 200

typedef uint8_t byte;
typedef struct pt_image pt_image;
typedef struct pt_image_vga pt_image_vga;

struct pt_image_vga {
    byte* bitmap;
    byte* mask;
    uint16_t width;
    uint16_t height;
    uint16_t pitch;
};

void video_init();
void video_clear();
void video_blit_image(pt_image* image, int16_t x, int16_t y);
bool video_is_vblank();
void video_flip();
void video_load_palette_colour(int idx);
uint8_t video_map_colour(uint8_t r, uint8_t g, uint8_t b);
pt_image_vga* video_convert_image(pt_image* image);
void video_destroy_hw_image(void* hw_image);
void video_shutdown();

bool sys_idle(int (*idle_callback)(), int idle_callback_period);

void timer_init();
uint32_t timer_ticks();
uint32_t timer_millis();
void timer_sleep(uint32_t delay_millis);
void timer_shutdown();

void pcspeaker_tone(float freq);
void pcspeaker_stop();

void mouse_init();
void mouse_update();
int mouse_is_down(int button);
int mouse_get_x();
int mouse_get_y();
void mouse_shutdown();

enum { MOUSE_BUTTON_LEFT, MOUSE_BUTTON_RIGHT, MOUSE_BUTTON_MIDDLE, MOUSE_BUTTON_MAX };

void keyboard_init();
void keyboard_set_key_repeat(bool allow);
bool keyboard_is_down(const char* key);
void keyboard_update();
void keyboard_shutdown();

void serial_init();
bool serial_rx_ready();
bool serial_tx_ready();
byte serial_getc();
int serial_gets(byte* buffer, size_t length);
void serial_putc(byte data);
size_t serial_write(const void* buffer, size_t size);
int serial_printf(const char* format, ...);
void serial_test();
void serial_shutdown();

#endif
