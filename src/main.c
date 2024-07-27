#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include "stb/stb_ds.h"

#include "dos.h"
#include "font.h"
#include "log.h"
#include "image.h"
#include "script.h"
#include "text.h"

int main(int argc, char **argv) {
    log_init();
    //pt_font *font = create_font("eagle.fnt");
    //const char *payload = "testing alignment of a shitload of text"; 
    //pt_text *text = create_text((byte *)payload, strlen(payload), font, 200, ALIGN_LEFT);
    timer_init();
    video_init();
    mouse_init();
    script_init();
    bool running = true;

    while (running) {
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
        mouse_update();
        script_draw();
    }
    //serial_test();
/*    while (script_exec()) {
        timer_sleep(10);
    }*/



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
    destroy_image(image);
    timer_sleep(1000);
    printf("Render cycle took %d millis", after - before);*/
    
    video_shutdown();
    timer_shutdown();
    log_shutdown();
    return 0;
}
