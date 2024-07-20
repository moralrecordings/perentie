#ifndef PERENTIE_DOS_H
#define PERENTIE_DOS_H

#include <stdbool.h>
#include <stdint.h>
#define SCREEN_WIDTH 320
#define SCREEN_HEIGHT 200

struct image;

void video_init();
void video_blit_image(struct image *image, int16_t x, int16_t y);
void video_shutdown();

void timer_init();
uint32_t timer_ticks();
uint32_t timer_millis();
void timer_sleep(uint32_t delay_millis);
void timer_shutdown();

void pcspeaker_tone(float freq);
void pcspeaker_stop();

void serial_test();

#endif
