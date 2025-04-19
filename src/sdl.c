#include <SDL3/SDL.h>
#include <SDL3/SDL_audio.h>
#include <SDL3/SDL_error.h>
#include <SDL3/SDL_events.h>
#include <SDL3/SDL_filesystem.h>
#include <SDL3/SDL_init.h>
#include <SDL3/SDL_keycode.h>
#include <SDL3/SDL_messagebox.h>
#include <SDL3/SDL_pixels.h>
#include <SDL3/SDL_render.h>
#include <SDL3/SDL_surface.h>
#include <SDL3/SDL_video.h>

#include "dos.h"
#include "event.h"
#include "image.h"
#include "log.h"
#include "rect.h"
#include "script.h"
#include "sdl.h"
#include "system.h"
#include "utils.h"

static SDL_AudioDeviceID audio_out = 0;
static SDL_Window* window = NULL;
static SDL_Renderer* renderer = NULL;

static SDL_Texture* framebuffer = NULL;

void sdl_init()
{
    if (!SDL_InitSubSystem(SDL_INIT_VIDEO | SDL_INIT_AUDIO)) {
        log_print("sdl_init: Failed to init: %s\n", SDL_GetError());
        return;
    }
    audio_out = SDL_OpenAudioDevice(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, NULL);
    if (!audio_out) {
        log_print("sdl_init: Failed to open audio device: %s\n", SDL_GetError());
    }
}

void sdl_set_meta(const char* name, const char* version, const char* identifier)
{
    // This command has to be run before basically any init steps
    SDL_SetAppMetadata(name, version, identifier);
}

char* sdl_get_data_path()
{
    char* path = SDL_GetPrefPath(NULL, SDL_GetAppMetadataProperty(SDL_PROP_APP_METADATA_IDENTIFIER_STRING));
    char* buffer = (char*)calloc(strlen(path) + 1, sizeof(char));
    memcpy(buffer, path, strlen(path));
    SDL_free(path);
    return buffer;
}

void sdl_shutdown()
{
    SDL_QuitSubSystem(SDL_INIT_VIDEO | SDL_INIT_AUDIO);
}

pt_drv_app sdl_app = { &sdl_init, &sdl_set_meta, &sdl_get_data_path, &sdl_shutdown };

void sdlvideo_shutdown();

void sdlvideo_init()
{
    if (window)
        return;

    const char* name = SDL_GetAppMetadataProperty(SDL_PROP_APP_METADATA_NAME_STRING);
    if (strcmp(name, "SDL Application") == 0)
        name = "Perentie";

    log_print("sdlvideo_init: Available render drivers:\n");
    for (int i = 0; i < SDL_GetNumRenderDrivers(); i++) {
        log_print("%d - %s\n", i, SDL_GetRenderDriver(i));
    }

    window = SDL_CreateWindow(name, SCREEN_WIDTH * 3, SCREEN_HEIGHT * 3, SDL_WINDOW_RESIZABLE);
    if (!window) {
        log_print("sdlvideo_init: Failed to create window: %s\n", SDL_GetError());
        SDL_QuitSubSystem(SDL_INIT_VIDEO);
        return;
    }
    renderer = SDL_CreateRenderer(window, "software");
    if (!renderer) {
        log_print("sdlvideo_init: Failed to create renderer: %s\n", SDL_GetError());
        SDL_DestroyWindow(window);
        window = NULL;
        SDL_QuitSubSystem(SDL_INIT_VIDEO);
        return;
    }
    SDL_SetRenderLogicalPresentation(renderer, SCREEN_WIDTH, SCREEN_HEIGHT, SDL_LOGICAL_PRESENTATION_INTEGER_SCALE);
    framebuffer = SDL_CreateTexture(
        renderer, SDL_GetWindowPixelFormat(window), SDL_TEXTUREACCESS_TARGET, SCREEN_WIDTH, SCREEN_HEIGHT);
    SDL_SetTextureScaleMode(framebuffer, SDL_SCALEMODE_NEAREST);
    SDL_SetRenderTarget(renderer, framebuffer);
    SDL_SetRenderVSync(renderer, 1);
    SDL_HideCursor();

    atexit(sdlvideo_shutdown);
}

void sdlvideo_shutdown()
{
    if (!window)
        return;

    char* crash = script_crash_message();
    if (crash) {
        SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "", crash, window);
    }
    framebuffer = NULL;
    SDL_DestroyRenderer(renderer);
    renderer = NULL;
    SDL_DestroyWindow(window);
    window = NULL;
}

void sdlvideo_clear()
{
    if (!renderer)
        return;

    SDL_SetRenderDrawColor(renderer, 0, 0, 0, SDL_ALPHA_OPAQUE);
    SDL_RenderClear(renderer);
}

pt_image_sdl* sdlvideo_convert_image(pt_image* image)
{
    pt_image_sdl* result = (pt_image_sdl*)calloc(1, sizeof(pt_image_sdl));
    SDL_Surface* draw = SDL_CreateSurface(image->width, image->height, SDL_PIXELFORMAT_INDEX8);
    SDL_Palette* pal = SDL_CreatePalette(256);
    byte palette_map[256];
    for (int i = 0; i < 256; i++) {
        palette_map[i] = map_colour(image->palette[3 * i], image->palette[3 * i + 1], image->palette[3 * i + 2]);
        pal->colors[i].r = pt_sys.palette[palette_map[i]].r;
        pal->colors[i].g = pt_sys.palette[palette_map[i]].g;
        pal->colors[i].b = pt_sys.palette[palette_map[i]].b;
        pal->colors[i].a = ((i == image->colourkey) || (image->palette_alpha[i] == 0x00)) ? SDL_ALPHA_TRANSPARENT
                                                                                          : image->palette_alpha[i];
    }

    SDL_SetSurfacePalette(draw, pal);
    result->revision = pt_sys.palette_revision;
    for (int y = 0; y < draw->h; y++) {
        for (int x = 0; x < draw->w; x++) {
            byte pixel = image->data[y * image->pitch + x];
            ((uint8_t*)draw->pixels)[y * draw->pitch + x] = dither_calc(pixel, x, y);
        }
    }

    result->texture = SDL_CreateTextureFromSurface(renderer, draw);
    if (result->texture) {
        SDL_SetTextureBlendMode(result->texture, SDL_BLENDMODE_BLEND);
        SDL_SetTextureScaleMode(result->texture, SDL_SCALEMODE_NEAREST);
    } else {
        log_print("sdlvideo_convert_image: failed to create %dx%d texture for %s: %s\n", draw->w, draw->h, image->path,
            SDL_GetError());
    }
    SDL_DestroySurface(draw);
    SDL_DestroyPalette(pal);

    return result;
}

void sdlvideo_destroy_hw_image(void* hw_image)
{
    pt_image_sdl* image = (pt_image_sdl*)hw_image;
    if (!image)
        return;

    if (image->texture) {
        SDL_DestroyTexture(image->texture);
        image->texture = NULL;
    }

    free(image);
}
void sdlvideo_blit_image(
    pt_image* image, int16_t x, int16_t y, uint8_t flags, int16_t left, int16_t top, int16_t right, int16_t bottom)
{
    if (!renderer)
        return;

    if (image->hw_image && ((pt_image_sdl*)image->hw_image)->revision != pt_sys.palette_revision) {
        sdlvideo_destroy_hw_image(image->hw_image);
        image->hw_image = NULL;
    }

    if (!image->hw_image) {
        image->hw_image = sdlvideo_convert_image(image);
    }

    pt_image_sdl* hw_image = (pt_image_sdl*)image->hw_image;

    struct rect ir = { MIN(image->width, MAX(0, left)), MIN(image->height, MAX(0, top)),
        MIN(image->width, MAX(0, right)), MIN(image->height, MAX(0, bottom)) };

    struct rect crop = { 0, 0, SCREEN_WIDTH, SCREEN_HEIGHT };
    // log_print("Blitting %s to (%d,%d) %dx%d ->", image->path, x, y, image->width, image->height);

    // Constrain x and y to be an absolute start offset in screen space.
    // Crop the image rectangle, based on its location in screen space.
    if (!rect_blit_clip(&x, &y, &ir, &crop)) {
        // log_print("Rectangle off screen\n");
        return;
    }

    SDL_FlipMode flip = SDL_FLIP_NONE;
    if (flags & FLIP_H)
        flip |= SDL_FLIP_HORIZONTAL;
    if (flags & FLIP_V)
        flip |= SDL_FLIP_VERTICAL;

    // after the image rect has been clipped, flip it if required
    if (flags & FLIP_H) {
        int16_t tmp = ir.right;
        ir.right = image->width - ir.left;
        ir.left = image->width - tmp;
    }

    if (flags & FLIP_V) {
        int16_t tmp = ir.bottom;
        ir.bottom = image->height - ir.top;
        ir.top = image->height - tmp;
    }

    SDL_FRect srcrect = { ir.left, ir.top, rect_width(&ir), rect_height(&ir) };
    SDL_FRect dstrect = { x, y, rect_width(&ir), rect_height(&ir) };

    SDL_RenderTextureRotated(renderer, hw_image->texture, &srcrect, &dstrect, 0.0, NULL, flip);
}

void sdlvideo_blit_line(int16_t x0, int16_t y0, int16_t x1, int16_t y1, pt_colour_rgb* colour)
{
    if (!renderer)
        return;

    SDL_SetRenderDrawColor(renderer, colour->r, colour->g, colour->b, SDL_ALPHA_OPAQUE);
    SDL_RenderLine(renderer, (float)x0, (float)y0, (float)x1, (float)y1);
}

void sdlvideo_blit()
{
    if (!renderer)
        return;

    // SDL renderer manages the frame buffer for us
    SDL_SetRenderTarget(renderer, NULL);
    sdlvideo_clear();
    SDL_RenderTexture(renderer, framebuffer, NULL, NULL);
}

void sdlvideo_flip()
{
    if (!renderer)
        return;

    SDL_RenderPresent(renderer);
    SDL_SetRenderTarget(renderer, framebuffer);
}

void sdlvideo_update_colour(uint8_t idx)
{
    // Game is actually rendering in 32-bit colour,
    // we don't need to update state
    // set_dither_from_remapper(idx, &pt_sys.dither[idx]);
}

void sdlvideo_set_palette_remapper(enum pt_palette_remapper remapper)
{
}

pt_drv_video sdl_video
    = { &sdlvideo_init, &sdlvideo_shutdown, &sdlvideo_clear, &sdlvideo_blit_image, &sdlvideo_blit_line, &sdlvideo_blit,
          &sdlvideo_flip, &sdlvideo_update_colour, &sdlvideo_destroy_hw_image, &sdlvideo_set_palette_remapper };

void sdltimer_init()
{
}
void sdltimer_shutdown()
{
}
uint32_t sdltimer_ticks()
{
    return SDL_GetTicks();
}
uint32_t sdltimer_millis()
{
    return SDL_GetTicks();
}
void sdltimer_sleep(uint32_t millis)
{
    SDL_Delay(millis);
};
uint32_t sdltimer_add_callback(uint32_t interval, pt_timer_callback callback, void* param)
{
    return SDL_AddTimer(interval, callback, param);
}
bool sdltimer_remove_callback(uint32_t id)
{
    return SDL_RemoveTimer(id);
}

pt_drv_timer sdl_timer = { &sdltimer_init, &sdltimer_shutdown, &sdltimer_ticks, &sdltimer_millis, &sdltimer_sleep,
    &sdltimer_add_callback, &sdltimer_remove_callback };

bool sdl_input_system = false;
int mouse_x = 0;
int mouse_y = 0;
bool mouse_button_states[MOUSE_BUTTON_MAX];

#define KEY_MAX 232

bool keyboard_allow_key_repeat = false;
char keyboard_key_states[KEY_MAX];
uint8_t keyboard_flags = 0;

static const char* SDL_SCANCODES[] = { "", "", "", "", "a", "b", "c", "d", "e", "f", "g", "h", "i", "j", "k", "l", "m",
    "n", "o", "p", "q", "r", "s", "t", "u", "v", "w", "x", "y", "z", "1", "2", "3", "4", "5", "6", "7", "8", "9", "0",
    "return", "escape", "backspace", "tab", "space", "-", "=", "[", "]", "\\", "", ";", "'", "`", ",", ".", "/",
    "capslock", "f1", "f2", "f3", "f4", "f5", "f6", "f7", "f8", "f9", "f10", "f11", "f12", "printscreen", "scrollock",
    "pause", "insert", "home", "pageup", "delete", "end", "pagedown", "right", "left", "down", "up", "numlockclear",
    "kpdivide", "kpmultiply", "kpminus", "kpplus", "kpenter", "kp1", "kp2", "kp3", "kp4", "kp5", "kp6", "kp7", "kp8",
    "kp9", "kp0", "kpperiod", "", "", "", "", "", "", "", "", "", "", "", "", "", "", "", "", "", "", "", "", "", "",
    "", "", "", "", "", "", "", "", "", "", "", "", "", "", "", "", "", "", "", "", "", "", "", "", "", "", "", "", "",
    "", "", "", "", "", "", "", "", "", "", "", "", "", "", "", "", "", "", "", "", "", "", "", "", "", "", "", "", "",
    "", "", "", "", "", "", "", "", "", "", "", "", "", "", "", "", "", "", "", "", "", "", "", "", "", "", "", "", "",
    "", "", "", "", "", "", "", "", "", "", "", "", "", "", "", "lctrl", "lshift", "lalt", "lgui", "rctrl", "rshift",
    "ralt", "rgui", NULL };

void sdlkeyboard_init()
{
    if (!sdl_input_system) {
        SDL_InitSubSystem(SDL_INIT_EVENTS);
        sdl_input_system = true;
    }
}
void sdlkeyboard_update()
{
    SDL_PumpEvents();
    SDL_Event ev;
    while (SDL_PollEvent(&ev)) {
        switch (ev.type) {
        case SDL_EVENT_QUIT:
            event_push(EVENT_QUIT);
            break;
        case SDL_EVENT_MOUSE_MOTION: {
            SDL_ConvertEventToRenderCoordinates(renderer, &ev);
            pt_event* t = event_push(EVENT_MOUSE_MOVE);
            t->mouse.x = (int)ev.motion.x;
            t->mouse.y = (int)ev.motion.y;
            t->mouse.dx = (int)ev.motion.xrel;
            t->mouse.dy = (int)ev.motion.yrel;
            mouse_x = t->mouse.x;
            mouse_y = t->mouse.y;
        } break;
        case SDL_EVENT_MOUSE_BUTTON_DOWN:
        case SDL_EVENT_MOUSE_BUTTON_UP:
            SDL_ConvertEventToRenderCoordinates(renderer, &ev);
            if (ev.button.button <= SDL_BUTTON_RIGHT) {
                pt_event* t = event_push(ev.type == SDL_EVENT_MOUSE_BUTTON_DOWN ? EVENT_MOUSE_DOWN : EVENT_MOUSE_UP);
                switch (ev.button.button) {
                case SDL_BUTTON_LEFT:
                    t->mouse.button = 1 << 0;
                    mouse_button_states[0] = ev.type == SDL_EVENT_MOUSE_BUTTON_DOWN;
                    break;
                case SDL_BUTTON_MIDDLE:
                    t->mouse.button = 1 << 2;
                    mouse_button_states[2] = ev.type == SDL_EVENT_MOUSE_BUTTON_DOWN;
                    break;
                case SDL_BUTTON_RIGHT:
                    t->mouse.button = 1 << 1;
                    mouse_button_states[1] = ev.type == SDL_EVENT_MOUSE_BUTTON_DOWN;
                    break;
                default:
                    break;
                }
                t->mouse.x = (int)ev.button.x;
                t->mouse.y = (int)ev.button.y;
            }
            break;
        case SDL_EVENT_KEY_DOWN:
        case SDL_EVENT_KEY_UP:
            if (ev.key.scancode < KEY_MAX) {
                pt_event* t = event_push(ev.type == SDL_EVENT_KEY_DOWN ? EVENT_KEY_DOWN : EVENT_KEY_UP);
                t->keyboard.key = SDL_SCANCODES[ev.key.scancode];
                t->keyboard.isrepeat = ev.key.repeat && keyboard_allow_key_repeat;
                if (ev.key.mod & SDL_KMOD_CTRL)
                    t->keyboard.flags |= KEY_FLAG_CTRL;
                if (ev.key.mod & SDL_KMOD_ALT)
                    t->keyboard.flags |= KEY_FLAG_ALT;
                if (ev.key.mod & SDL_KMOD_SHIFT)
                    t->keyboard.flags |= KEY_FLAG_SHIFT;
                if (ev.key.mod & SDL_KMOD_NUM)
                    t->keyboard.flags |= KEY_FLAG_NUM;
                if (ev.key.mod & SDL_KMOD_CAPS)
                    t->keyboard.flags |= KEY_FLAG_CAPS;
                if (ev.key.mod & SDL_KMOD_SCROLL)
                    t->keyboard.flags |= KEY_FLAG_SCRL;
            }
            break;
        default:
            break;
        }
    }
}
void sdlkeyboard_shutdown()
{
    if (sdl_input_system) {
        SDL_QuitSubSystem(SDL_INIT_EVENTS);
        sdl_input_system = false;
    }
}
void sdlkeyboard_set_key_repeat(bool allow)
{
    keyboard_allow_key_repeat = allow;
}
bool sdlkeyboard_is_key_down(const char* key)
{
    for (int i = 0; SDL_SCANCODES[i]; i++) {
        if (!strcmp(SDL_SCANCODES[i], key)) {
            return keyboard_key_states[i] == 1;
        }
    }
    return false;
}

pt_drv_keyboard sdl_keyboard = { &sdlkeyboard_init, &sdlkeyboard_update, &sdlkeyboard_shutdown,
    &sdlkeyboard_set_key_repeat, &sdlkeyboard_is_key_down };

// handled by sdlkeyboard
void sdlmouse_init()
{
}
void sdlmouse_update()
{
}
void sdlmouse_shutdown()
{
}

int sdlmouse_get_x()
{
    return mouse_x;
}
int sdlmouse_get_y()
{
    return mouse_y;
}
bool sdlmouse_is_button_down(enum pt_mouse_button button)
{
    return mouse_button_states[button];
}

pt_drv_mouse sdl_mouse = { &sdlmouse_init, &sdlmouse_update, &sdlmouse_shutdown, &sdlmouse_get_x, &sdlmouse_get_y,
    &sdlmouse_is_button_down };

void sdlserial_init()
{
}
void sdlserial_shutdown()
{
}
void sdlserial_open_device(const char* device)
{
}
void sdlserial_close_device()
{
}
bool sdlserial_rx_ready()
{
    return true;
}
bool sdlserial_tx_ready()
{
    return true;
}
byte sdlserial_getc()
{
    return 0;
}
int sdlserial_gets(byte* byffer, size_t length)
{
    return 0;
}
void sdlserial_putc(byte data)
{
}
size_t sdlserial_write(const void* buffer, size_t size)
{
    return 0;
}
int sdlserial_printf(const char* format, ...)
{
    return 0;
}

pt_drv_serial sdl_serial
    = { &sdlserial_init, &sdlserial_shutdown, &sdlserial_open_device, &sdlserial_close_device, &sdlserial_rx_ready,
          &sdlserial_tx_ready, &sdlserial_getc, &sdlserial_gets, &sdlserial_putc, &sdlserial_write, &sdlserial_printf };

void sdlbeep_init()
{
}

void sdlbeep_shutdown()
{
}

void sdlbeep_tone(float freq)
{
}

void sdlbeep_play_sample(byte* data, size_t len, int rate)
{
    if (data)
        free(data);
}

void sdlbeep_play_data(uint16_t* data, size_t len, int rate)
{
    if (data)
        free(data);
}

void sdlbeep_stop()
{
}

pt_drv_beep sdl_beep
    = { &sdlbeep_init, &sdlbeep_shutdown, &sdlbeep_tone, &sdlbeep_play_sample, &sdlbeep_play_data, &sdlbeep_stop };

// For some reason you can't actually include woodyopl/opl.h, as it has a bunch of
// player state embedded into it. Oh well!
extern void adlib_init(uint32_t samplerate);
extern void adlib_write(uintptr_t idx, uint8_t val);
extern void adlib_getsample(int16_t* sndptr, intptr_t numsamples);

#define OPL_RATE 49716
#define OPL_BUFFER 128
static SDL_AudioStream* oploutput = NULL;
static bool oplinited = false;

void sdlopl_callback(void* userdata, SDL_AudioStream* stream, int additional_amount, int total_amount)
{
    int samples_needed = additional_amount > 0 ? ((additional_amount + OPL_BUFFER * 8) >> 2) : 0;
    int bytes_given = 0;
    if (samples_needed > 0) {
        // So the trick here is that the callback will be asked for ~4616 bytes at a time.
        // However if we try and render the whole thing ahead of time, at about the 80% mark
        // the audio pipeline will run out of data and start mixing in silence, which causes audible pops.
        // The solution is to cut the request into tiny slices so that the pipeline is always full.
        int16_t samples[OPL_BUFFER << 1];
        while (samples_needed > 0) {
            int sample_count = MIN(OPL_BUFFER, samples_needed);
            adlib_getsample(samples, sample_count);
            SDL_PutAudioStreamData(stream, samples, sizeof(int16_t) * (sample_count << 1));
            samples_needed -= sample_count;
            bytes_given += sizeof(int16_t) * (sample_count << 1);
        }
    }
    // printf("sdlopl_callback: %d additional, %d total, %d given\n", additional_amount, total_amount, bytes_given);
}

void sdlopl_init()
{
    if (oploutput)
        return;

    if (!audio_out)
        return;

    if (!oplinited) {
        adlib_init(OPL_RATE);
        oplinited = true;
    }

    SDL_AudioSpec src = { SDL_AUDIO_S16LE, 2, OPL_RATE };
    oploutput = SDL_CreateAudioStream(&src, NULL);
    SDL_SetAudioStreamGetCallback(oploutput, sdlopl_callback, NULL);
    if (!SDL_BindAudioStream(audio_out, oploutput)) {
        log_print("sdlopl_init: Failed to bind audio stream to device: %s\n", SDL_GetError());
    }
}

void sdlopl_shutdown()
{
    if (!oploutput)
        return;
    SDL_DestroyAudioStream(oploutput);
    oploutput = NULL;
    oplinited = false;
}

void sdlopl_write_reg(uint16_t addr, uint8_t data)
{
    if (!oplinited) {
        adlib_init(OPL_RATE);
        oplinited = true;
    }

    // Lock the audio stream mutex, aka. block the callback from running
    if (oploutput)
        SDL_LockAudioStream(oploutput);
    adlib_write(addr, data);
    if (oploutput)
        SDL_UnlockAudioStream(oploutput);
}

bool sdlopl_is_ready()
{
    if (!oploutput)
        return false;

    return true;
}

pt_drv_opl sdl_opl = { &sdlopl_init, &sdlopl_shutdown, &sdlopl_write_reg, &sdlopl_is_ready };
