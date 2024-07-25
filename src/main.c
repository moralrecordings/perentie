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
    bool running = true;

    while (!running) {
        // sleep until the start of the next vertical blanking interval
        if (video_is_vblank()) {
            // in the middle of a vblank, wait until the next draw 
            do {
                running = sys_idle(script_exec, 10);
            } while (video_is_vblank());
        }
        // in the middle of a draw, wait for the start of the next vblank
        do {
            running = sys_idle(script_exec, 10);
        } while (!video_is_vblank());
        video_flip();
        script_draw();
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
    timer_sleep(1000);
    printf("Render cycle took %d millis", after - before);*/
    timer_shutdown();
    return 0;
}
