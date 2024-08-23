#include "stb/stb_ds.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "dos.h"
#include "event.h"
#include "log.h"
#include "script.h"
#include "system.h"

#include "musicrad.h"

int main(int argc, char** argv)
{
#ifdef SYSTEM_DOS
    pt_sys.timer = &dos_timer;
    pt_sys.keyboard = &dos_keyboard;
    pt_sys.mouse = &dos_mouse;
    pt_sys.serial = &dos_serial;
    pt_sys.opl = &dos_opl;
    pt_sys.beep = &dos_beep;
    pt_sys.video = &dos_vga;
#else
    return 0;
#endif

    log_init();
    event_init();
    pt_sys.timer->init();
    pt_sys.beep->init();
    pt_sys.opl->init();
    pt_sys.serial->init();
    pt_sys.video->init();
    pt_sys.mouse->init();
    pt_sys.keyboard->init();
    radplayer_init();
    script_init();
    bool running = true;

    while (!script_has_quit()) {
        // sleep until the start of the next vertical blanking interval
        if (pt_sys.video->is_vblank()) {
            // in the middle of a vblank, wait until the next draw
            do {
                running = sys_idle(script_exec, 10);
                radplayer_update();
            } while (pt_sys.video->is_vblank());
        }
        // in the middle of a draw, wait for the start of the next vblank
        do {
            running = sys_idle(script_exec, 10);
            radplayer_update();
        } while (!pt_sys.video->is_vblank());
        pt_sys.video->flip();
        pt_sys.keyboard->update();
        pt_sys.mouse->update();
        script_events();
        script_render();
        script_repl();
    }

    radplayer_shutdown();
    pt_sys.video->shutdown();
    pt_sys.mouse->shutdown();
    pt_sys.keyboard->shutdown();
    pt_sys.serial->shutdown();
    pt_sys.opl->shutdown();
    pt_sys.beep->shutdown();
    pt_sys.timer->shutdown();
    event_shutdown();
    log_shutdown();
    return script_quit_status();
}
