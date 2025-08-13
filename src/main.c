#include <stdlib.h>
#include <time.h>
#include <unistd.h>

#include "argparse/argparse.h"

#include "colour.h"
#include "event.h"
#include "fs.h"
#include "log.h"
#include "musicrad.h"
#include "pcspeak.h"
#include "script.h"
#include "system.h"
#include "version.h"

#ifdef SYSTEM_SDL
#include "sdl.h"
#endif

#ifdef SYSTEM_DOS
#include "dos.h"
#endif

#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#endif

static uint32_t samples[16] = { 0 };
static uint32_t draws[16] = { 0 };
static uint32_t blits[16] = { 0 };
static uint32_t flips[16] = { 0 };
static uint32_t sample_idx = 0;

static const char* const usages[] = {
    "perentie [--version] [--debug] PATH [PATH ...]",
    NULL,
};

void perentie_init(int argc, const char** argv)
{
#ifdef SYSTEM_DOS
    pt_sys.app = &dos_app;
    pt_sys.timer = &dos_timer;
    pt_sys.keyboard = &dos_keyboard;
    pt_sys.mouse = &dos_mouse;
    pt_sys.serial = &dos_serial;
    pt_sys.opl = &dos_opl;
    pt_sys.beep = &dos_beep;
    pt_sys.video = &dos_vga;
#elif SYSTEM_SDL
    pt_sys.app = &sdl_app;
    pt_sys.timer = &sdl_timer;
    pt_sys.keyboard = &sdl_keyboard;
    pt_sys.mouse = &sdl_mouse;
    pt_sys.serial = &sdl_serial;
    pt_sys.opl = &sdl_opl;
    pt_sys.beep = &sdl_beep;
    pt_sys.video = &sdl_video;
#else
    exit(1);
#endif

    const char* argv0 = argc > 0 ? argv[0] : "perentie";
    int version = 0;
    int log = 0;
    struct argparse_option options[]
        = { OPT_HELP(), OPT_BOOLEAN('v', "version", &version, "print version and exit", NULL, 0, 0),
              OPT_BOOLEAN('l', "log", &log, "run with debug logging", NULL, 0, 0), OPT_END() };
    struct argparse argparse;
    argparse_init(&argparse, options, usages, 0);
    argc = argparse_parse(&argparse, argc, argv);
    if (version) {
        printf("Perentie %s (%s)\n", VERSION, PLATFORM);
        exit(0);
    }

    // seed the RNG in the most basic way
    srand(time(NULL));

    // Initialise driver system
    pt_sys.app->init();

    log_init(log);
    fs_init(argv0, argc, argv);
    radplayer_init();
    event_init();

    if (!fs_exists("main.lua")) {
        printf("main.lua not found! Perentie needs a main.lua file in order to start.\n");
        exit(1);
    }

    script_init();
    palette_init();

    pt_sys.timer->init();
    pt_sys.beep->init();
    pt_sys.opl->init();
    pt_sys.serial->init();
    pt_sys.video->init();
    pt_sys.mouse->init();
    pt_sys.keyboard->init();
}

void perentie_shutdown()
{
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
    pt_sys.app->shutdown();
    script_shutdown();
    event_shutdown();
    log_shutdown();
    fs_shutdown();
}

static void perentie_loop()
{
    // Main löp
    if (script_has_quit()) {
        perentie_shutdown();
#ifdef __EMSCRIPTEN__
        emscripten_cancel_main_loop(); /* this should "kill" the app. */
        return;
#else
        exit(script_quit_status());
#endif
    }
    uint32_t ticks = pt_sys.timer->millis();
    // Run Lua coroutines for 1 step
    script_exec();
    // Rack up any input events
    pt_sys.keyboard->update();
    pt_sys.mouse->update();
    // Process input events in Lua
    script_events();
    samples[sample_idx] = pt_sys.timer->millis() - ticks;

    ticks = pt_sys.timer->millis();
    // Run Lua routine for drawing graphics to framebuffer
    script_render();
    draws[sample_idx] = pt_sys.timer->millis() - ticks;

    ticks = pt_sys.timer->millis();
    // Copy framebuffer to video memory
    pt_sys.video->blit();
    blits[sample_idx] = pt_sys.timer->millis() - ticks;

    // Deal with input from the serial debug console
    script_repl();

    ticks = pt_sys.timer->millis();
    // Flip the video page and sync to display refresh rate
    pt_sys.video->flip();
    flips[sample_idx] = pt_sys.timer->millis() - ticks;
    sample_idx = (sample_idx + 1) % 16;
}

int main(int argc, const char** argv)
{
    perentie_init(argc, argv);

#ifdef __EMSCRIPTEN__
    emscripten_set_main_loop(perentie_loop, 0, 1);
#else
    while (true) {
        perentie_loop();
    }
#endif
    return 0;
}
