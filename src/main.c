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
    struct image *image = create_image("test.png");

    video_init();
    for (int i = 0; i < 100; i++) {
        video_blit_image(image, 
            (int)(rand()%(SCREEN_WIDTH + image->width)) - (int)image->width,
            (int)(rand()%(SCREEN_HEIGHT + image->height)) - (int)image->height
        );
        usleep(100000);
    }
    video_shutdown();
    destroy_image(image);
//    sleep(1);
//    luaL_dofile(L, "test.lua");

    return 0;
}
