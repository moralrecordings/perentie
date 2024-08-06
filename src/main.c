#include "stb/stb_ds.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "dos.h"
#include "event.h"
#include "image.h"
#include "log.h"
#include "script.h"
#include "text.h"

int main(int argc, char** argv)
{
    log_init();
    event_init();
    timer_init();
    serial_init();
    video_init();
    mouse_init();
    keyboard_init();
    script_init();
    bool running = true;
    while (!script_has_quit()) {
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
        keyboard_update();
        mouse_update();
        script_events();
        script_render();
        script_repl();
    }

    video_shutdown();
    mouse_shutdown();
    keyboard_shutdown();
    serial_shutdown();
    timer_shutdown();
    event_shutdown();
    log_shutdown();
    return script_quit_status();
}
