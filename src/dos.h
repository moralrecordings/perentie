#ifndef PERENTIE_DOS_H
#define PERENTIE_DOS_H

#include <stdbool.h>
#include <stdint.h>
#define SCREEN_WIDTH 320
#define SCREEN_HEIGHT 200

typedef struct pt_image pt_image;

void video_init();
void video_clear();
void video_blit_image(pt_image *image, int16_t x, int16_t y);
bool video_is_vblank();
void video_flip();
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
void mouse_shutdown();

void serial_test();

#endif
