#ifndef PERENTIE_SDL_H
#define PERENTIE_SDL_H

#include <stdbool.h>
#include <stdint.h>

#define SCREEN_WIDTH 320
#define SCREEN_HEIGHT 200
#define SCREEN_SCALE 3

#include "system.h"

typedef struct SDL_Texture SDL_Texture;
typedef struct pt_image pt_image;
typedef struct pt_image_sdl pt_image_sdl;

struct pt_image_sdl {
    SDL_Texture* texture;
    int revision;
};

void sdl_init();
void sdl_shutdown();

extern pt_drv_video sdl_video;

extern pt_drv_timer sdl_timer;

extern pt_drv_beep sdl_beep;

extern pt_drv_mouse sdl_mouse;

extern pt_drv_keyboard sdl_keyboard;

extern pt_drv_serial sdl_serial;

extern pt_drv_opl sdl_opl;

#endif
