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

    uint32_t samples[16] = { 0 };
    uint32_t sample_idx = 0;

    while (!script_has_quit()) {
        uint32_t ticks = pt_sys.timer->millis();
        script_exec();
        radplayer_update();
        pt_sys.keyboard->update();
        pt_sys.mouse->update();
        script_events();
        script_render();
        pt_sys.video->blit();
        script_repl();
        samples[sample_idx] = pt_sys.timer->millis() - ticks;
        sample_idx = (sample_idx + 1) % 16;

        pt_sys.video->flip();
    }

    log_print("Last frame times: %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d\n", samples[0], samples[1], samples[2],
        samples[3], samples[4], samples[5], samples[6], samples[7], samples[8], samples[9], samples[10], samples[11],
        samples[12], samples[13], samples[14], samples[15]);

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
