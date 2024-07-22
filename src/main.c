#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include "stb/stb_ds.h"

#include "dos.h"
#include "image.h"
#include "script.h"

int main(int argc, char **argv) {
    timer_init();
    script_init();

    while (script_exec()) {
        timer_sleep(10);
    }

    //serial_test();

    /*
    struct image *image = create_image("test.png");

    video_init();
    uint32_t before = timer_millis();
    for (int i = 0; i < 500; i++) {
        video_blit_image(image, 
            (int)(rand()%(SCREEN_WIDTH + image->width)) - (int)image->width,
            (int)(rand()%(SCREEN_HEIGHT + image->height)) - (int)image->height
        );
        //timer_sleep(1000/70);
        video_flip();
    }
    uint32_t after = timer_millis();
    video_shutdown();
    destroy_image(image);
//    luaL_dofile(L, "test.lua");
    timer_sleep(1000);
    printf("Render cycle took %d millis", after - before);*/
    timer_shutdown();
    return 0;
}
