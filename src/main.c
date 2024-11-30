#include "stb/stb_ds.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#ifdef SYSTEM_DOS
#include <dpmi.h>
#endif

#include "colour.h"
#include "dos.h"
#include "event.h"
#include "log.h"
#include "script.h"
#include "system.h"

#include "musicrad.h"

#ifdef SYSTEM_DOS
extern char etext;
#endif

int main(int argc, char** argv)
{

#ifdef SYSTEM_DOS
    // Lock data so that pt_sys is accessible from interrupts
    LOCK_DATA(pt_sys)
    LOCK_DATA(dos_timer)
    LOCK_DATA(dos_keyboard)
    LOCK_DATA(dos_mouse)
    LOCK_DATA(dos_serial)
    LOCK_DATA(dos_opl)
    LOCK_DATA(dos_beep)
    LOCK_DATA(dos_vga)

    // NASTYHACK: As it turns out, there's no guarantee
    // with GCC that functions will be linked and stored in the same
    // order, and therefore no straightforward way of getting the
    // memory range of individual functions for locking.

    // We could use sections, if it weren't for the fact that CWSDPMI
    // is hardcoded to load .text and .data and nothing else.

    // Instead, just lock ALL of .text using the magic GCC linker symbol.
    // It's kind of big but I've stopped caring!
    _go32_dpmi_lock_code((void*)0x1000, (long)&etext - (long)0x1000);

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
    for (int i = 0; i < 16; i++) {
        pt_sys.palette[i].r = ega_palette[i].r;
        pt_sys.palette[i].g = ega_palette[i].g;
        pt_sys.palette[i].b = ega_palette[i].b;
    }
    pt_sys.palette_top = 16;

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
    uint32_t draws[16] = { 0 };
    uint32_t blits[16] = { 0 };
    uint32_t flips[16] = { 0 };
    uint32_t sample_idx = 0;

    while (!script_has_quit()) {
        uint32_t ticks = pt_sys.timer->millis();
        script_exec();
        pt_sys.keyboard->update();
        pt_sys.mouse->update();
        script_events();
        samples[sample_idx] = pt_sys.timer->millis() - ticks;

        ticks = pt_sys.timer->millis();
        script_render();
        draws[sample_idx] = pt_sys.timer->millis() - ticks;

        ticks = pt_sys.timer->millis();
        pt_sys.video->blit();
        blits[sample_idx] = pt_sys.timer->millis() - ticks;

        script_repl();

        ticks = pt_sys.timer->millis();
        pt_sys.video->flip();
        flips[sample_idx] = pt_sys.timer->millis() - ticks;
        sample_idx = (sample_idx + 1) % 16;
    }

    log_print("Last frame times: %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d\n", samples[0], samples[1], samples[2],
        samples[3], samples[4], samples[5], samples[6], samples[7], samples[8], samples[9], samples[10], samples[11],
        samples[12], samples[13], samples[14], samples[15]);

    log_print("Last FB draw times: %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d\n", draws[0], draws[1], draws[2],
        draws[3], draws[4], draws[5], draws[6], draws[7], draws[8], draws[9], draws[10], draws[11], draws[12],
        draws[13], draws[14], draws[15]);

    log_print("Last FB copy times: %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d\n", blits[0], blits[1], blits[2],
        blits[3], blits[4], blits[5], blits[6], blits[7], blits[8], blits[9], blits[10], blits[11], blits[12],
        blits[13], blits[14], blits[15]);

    log_print("Last flip times: %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d\n", flips[0], flips[1], flips[2],
        flips[3], flips[4], flips[5], flips[6], flips[7], flips[8], flips[9], flips[10], flips[11], flips[12],
        flips[13], flips[14], flips[15]);

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
