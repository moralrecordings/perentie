#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include "lua/lua.h"
#include "lua/lauxlib.h"
#include "lua/lualib.h"

#include "dos.h"
#include "image.h"

int main(int argc, char **argv) {
    lua_State *L = luaL_newstate();
    luaL_openlibs(L);

    timer_init();

    for (int j = 1; j < 20; j++) {
        int delay = 250/j;
        printf("Loop %d, delay %d\n", j, delay);
        for (int i = 0; i < 24; i++) {
            float freq = 220.0f * powf(2, i/12.0f); 
            pcspeaker_tone(freq);
            timer_sleep(delay);
            pcspeaker_stop();
        }
    }

    //serial_test();

    struct image *image = create_image("test.png");

    video_init();
    for (int i = 0; i < 140; i++) {
        video_blit_image(image, 
            (int)(rand()%(SCREEN_WIDTH + image->width)) - (int)image->width,
            (int)(rand()%(SCREEN_HEIGHT + image->height)) - (int)image->height
        );
        timer_sleep(1000/70);
    }
    video_shutdown();
    timer_shutdown();
    destroy_image(image);
//    sleep(1);
//    luaL_dofile(L, "test.lua");

    return 0;
}
