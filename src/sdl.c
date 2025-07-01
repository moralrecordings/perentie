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
#include "fs.h"
#include "image.h"
#include "log.h"
#include "pcspeak.h"
#include "rect.h"
#include "script.h"
#include "sdl.h"
#include "system.h"
#include "utils.h"
#include "woodypc/pcspeak.h"

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

char* sdl_get_data_path()
{
    char* path = SDL_GetPrefPath(NULL, SDL_GetAppMetadataProperty(SDL_PROP_APP_METADATA_IDENTIFIER_STRING));
    char* buffer = (char*)calloc(strlen(path) + 1, sizeof(char));
    memcpy(buffer, path, strlen(path));
    SDL_free(path);
    return buffer;
}

void sdl_set_meta(const char* name, const char* version, const char* identifier)
{
    // This command has to be run before basically any init steps
    SDL_SetAppMetadata(name, version, identifier);
    char* path = sdl_get_data_path();
    fs_set_write_dir(path);
    free(path);
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

    pt_colour_rgb* fill = &pt_sys.palette[pt_sys.overscan];
    SDL_SetRenderDrawColor(renderer, fill->r, fill->g, fill->b, SDL_ALPHA_OPAQUE);
    SDL_RenderClear(renderer);
}

pt_image_sdl* sdlvideo_convert_image(pt_image* image)
{
    pt_image_sdl* result = (pt_image_sdl*)calloc(1, sizeof(pt_image_sdl));
    SDL_Surface* draw = SDL_CreateSurface(image->width, image->height, SDL_PIXELFORMAT_INDEX8);

    // Create a mapping between image colours and global palette
    byte palette_map[256];
    for (int i = 0; i < 256; i++) {
        uint8_t src = (image->palette_alpha[i] == 0)
            ? 0xff
            : map_colour(image->palette[3 * i], image->palette[3 * i + 1], image->palette[3 * i + 2]);
        palette_map[i] = src;
    }

    // Create an image palette based on the global palette.
    SDL_Palette* pal = SDL_CreatePalette(256);
    for (int i = 0; i < 255; i++) {
        pal->colors[i].r = pt_sys.palette[i].r;
        pal->colors[i].g = pt_sys.palette[i].g;
        pal->colors[i].b = pt_sys.palette[i].b;
        pal->colors[i].a = 0xff;
    }
    // The 256th colour is used to indicate transparency, as that's how SDL rolls.
    // This may be an issue if we completely deplete the palette space.
    pal->colors[255].r = 0x00;
    pal->colors[255].g = 0x00;
    pal->colors[255].b = 0x00;
    pal->colors[255].a = 0x00;

    SDL_SetSurfacePalette(draw, pal);
    result->revision = pt_sys.palette_revision;
    for (int y = 0; y < draw->h; y++) {
        for (int x = 0; x < draw->w; x++) {
            byte pixel = image->data[y * image->pitch + x];
            if (palette_map[pixel] == 0xff || (pixel == image->colourkey)) {
                ((uint8_t*)draw->pixels)[y * draw->pitch + x] = 0xff;
            } else {
                ((uint8_t*)draw->pixels)[y * draw->pitch + x] = dither_calc(palette_map[pixel], x, y);
            }
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

    // if (image->path && strcmp(image->path, "assets/bridge/bridge.png") == 0)
    //     log_print("Blitting %s to (%d,%d) %dx%d ->", image->path, x, y, image->width, image->height);

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

    // if (image->path && strcmp(image->path, "assets/bridge/bridge.png") == 0)
    //     log_print("(%d,%d) (%d,%d) %dx%d l=%d, r=%d\n", ir.left, ir.top, x, y, rect_width(&ir), rect_height(&ir),
    //     left, right);

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

void sdlvideo_update_palette_slot(uint8_t idx)
{
    // Game is actually rendering in 32-bit colour,
    // we don't need to update state
    // set_dither_from_remapper(idx, &pt_sys.dither[idx]);
}

void sdlvideo_set_palette_remapper(enum pt_palette_remapper remapper, enum pt_palette_remapper_mode mode)
{
    pt_sys.remapper = remapper;
    pt_sys.remapper_mode = mode;
    for (int i = 0; i < pt_sys.palette_top; i++) {
        set_dither_from_remapper(remapper, mode, i, &pt_sys.dither[i]);
    }

    // Force all hardware images to be recalculated
    pt_sys.palette_revision++;
}

void sdlvideo_set_overscan_colour(pt_colour_rgb* colour)
{
}

pt_drv_video sdl_video = { &sdlvideo_init, &sdlvideo_shutdown, &sdlvideo_clear, &sdlvideo_blit_image,
    &sdlvideo_blit_line, &sdlvideo_blit, &sdlvideo_flip, &sdlvideo_update_palette_slot, &sdlvideo_destroy_hw_image,
    &sdlvideo_set_palette_remapper, &sdlvideo_set_overscan_colour };

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
    uint32_t result = SDL_AddTimer(interval, callback, param);
    if (result == 0) {
        log_print("sdltimer_add_callback: Failed to add a timer, platform probably has broken threads - %s\n",
            SDL_GetError());
    }
    return result;
}
bool sdltimer_remove_callback(uint32_t id)
{
    bool result = SDL_RemoveTimer(id);
    return result;
}

bool sdltimer_supports_hires()
{
    return true;
}
bool sdltimer_get_hires()
{
    return true;
}
void sdltimer_set_hires(bool enabled) { };

pt_drv_timer sdl_timer
    = { &sdltimer_init, &sdltimer_shutdown, &sdltimer_ticks, &sdltimer_millis, &sdltimer_sleep, &sdltimer_add_callback,
          &sdltimer_remove_callback, &sdltimer_supports_hires, &sdltimer_get_hires, &sdltimer_set_hires };

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
        // By default, SDL will make fake mouse events for any touch events.
        // Let's just do that ourselves.
        SDL_SetHint(SDL_HINT_TOUCH_MOUSE_EVENTS, "0");
        sdl_input_system = true;
    }
}

#define DRAG_THRESHOLD 3.0
#define PRESS_TIME 500

static bool using_touch = false;
static bool in_touch_press = false;
static bool touch_has_resolved = false;
static bool in_touch_drag = false;
static SDL_TouchFingerEvent touch_lastevent = { 0 };
static uint32_t touch_time = 0;

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
        // We want to support three scenarios with touch:
        // - Tapping the screen (same spot, < 500ms) -> left mouse down + left mouse up
        // - Long pressing on the screen (same spot, 500ms+) -> right mouse down + right mouse up
        // - Tapping and dragging on the screen -> left mouse down, followed by left mouse up when finger is released
        // The screen should always show the cursor moving, but taps and long presses should resolve
        // at the site of the finger down event.
        case SDL_EVENT_FINGER_DOWN: {
            using_touch = true;
            SDL_ConvertEventToRenderCoordinates(renderer, &ev);
            // log_print("SDL_EVENT_FINGER_DOWN: x: %f, y: %f, pressure: %f, touchID: %d, fingerID: %lld\n",
            // ev.tfinger.x,
            //     ev.tfinger.y, ev.tfinger.pressure, ev.tfinger.touchID, ev.tfinger.fingerID);
            if (!in_touch_press) {
                // log_print("finger!\n");
                in_touch_press = true;
                touch_has_resolved = false;
                in_touch_drag = false;
                memcpy(&touch_lastevent, &ev.tfinger, sizeof(SDL_TouchFingerEvent));
                touch_time = pt_sys.timer->millis();
                pt_event* t = event_push(EVENT_MOUSE_MOVE);
                t->mouse.dx = (int)ev.tfinger.x - mouse_x;
                t->mouse.dy = (int)ev.tfinger.y - mouse_y;
                mouse_x = (int)ev.tfinger.x;
                mouse_y = (int)ev.tfinger.y;
                t->mouse.x = mouse_x;
                t->mouse.y = mouse_y;

            } else {
                // we're already in a touch fingerdown event, do nothing
            }
        } break;
        case SDL_EVENT_FINGER_UP: {
            SDL_ConvertEventToRenderCoordinates(renderer, &ev);
            // log_print("SDL_EVENT_FINGER_UP: x: %f, y: %f, pressure: %f, touchID: %d, fingerID: %lld\n", ev.tfinger.x,
            //     ev.tfinger.y, ev.tfinger.pressure, ev.tfinger.touchID, ev.tfinger.fingerID);
            if (in_touch_press && ev.tfinger.touchID == touch_lastevent.touchID
                && ev.tfinger.fingerID == touch_lastevent.fingerID) {
                if (!touch_has_resolved) {
                    if (!in_touch_drag) {
                        // log_print("tap finished!\n");
                        pt_event* t = event_push(EVENT_MOUSE_MOVE);
                        t->mouse.dx = (int)ev.tfinger.x - mouse_x;
                        t->mouse.dy = (int)ev.tfinger.y - mouse_y;
                        mouse_x = (int)ev.tfinger.x;
                        mouse_y = (int)ev.tfinger.y;
                        t->mouse.x = mouse_x;
                        t->mouse.y = mouse_y;

                        t = event_push(EVENT_MOUSE_DOWN);
                        t->mouse.button = 1 << 0;
                        t->mouse.x = mouse_x;
                        t->mouse.y = mouse_x;
                    } else {
                        // log_print("drag finished!\n");
                    }
                    pt_event* t = event_push(EVENT_MOUSE_UP);
                    t->mouse.button = 1 << 0;
                    t->mouse.x = mouse_x;
                    t->mouse.y = mouse_y;
                }

                in_touch_press = false;
                touch_has_resolved = false;
                in_touch_drag = false;
                memset(&touch_lastevent, 0, sizeof(SDL_TouchFingerEvent));
            }
        } break;
        case SDL_EVENT_FINGER_MOTION: {
            SDL_ConvertEventToRenderCoordinates(renderer, &ev);
            // log_print("SDL_EVENT_FINGER_MOTION: x: %f, y: %f, pressure: %f, touchID: %d, fingerID: %lld\n",
            //     ev.tfinger.x, ev.tfinger.y, ev.tfinger.pressure, ev.tfinger.touchID, ev.tfinger.fingerID);
            if (in_touch_press && ev.tfinger.touchID == touch_lastevent.touchID
                && ev.tfinger.fingerID == touch_lastevent.fingerID) {
                if (!in_touch_drag && !touch_has_resolved) {
                    // Check if we've moved around enough
                    if ((abs((int)(ev.tfinger.x - touch_lastevent.x)) > DRAG_THRESHOLD)
                        || (abs((int)(ev.tfinger.y - touch_lastevent.y)) > DRAG_THRESHOLD)) {
                        // log_print("drag started!\n");
                        in_touch_drag = true;
                        // temporarily move the mouse to the start pos from the FINGER_DOWN event
                        pt_event* t = event_push(EVENT_MOUSE_MOVE);
                        t->mouse.dx = (int)touch_lastevent.x - mouse_x;
                        t->mouse.dy = (int)touch_lastevent.y - mouse_y;
                        mouse_x = (int)touch_lastevent.x;
                        mouse_y = (int)touch_lastevent.y;
                        t->mouse.x = mouse_x;
                        t->mouse.y = mouse_y;

                        t = event_push(EVENT_MOUSE_DOWN);
                        t->mouse.button = 1 << 0;
                        t->mouse.x = mouse_x;
                        t->mouse.y = mouse_y;
                    }
                }
                // Always show the cursor moving
                pt_event* t = event_push(EVENT_MOUSE_MOVE);
                t->mouse.dx = (int)ev.tfinger.x - mouse_x;
                t->mouse.dy = (int)ev.tfinger.y - mouse_y;
                mouse_x = (int)ev.tfinger.x;
                mouse_y = (int)ev.tfinger.y;
                t->mouse.x = mouse_x;
                t->mouse.y = mouse_y;
            }
        } break;
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

    // Simulate right mouse click input based on the touch state
    if (in_touch_press && !in_touch_drag && !touch_has_resolved) {
        if (pt_sys.timer->millis() - touch_time > PRESS_TIME) {
            touch_has_resolved = true;
            pt_event* t = event_push(EVENT_MOUSE_MOVE);
            t->mouse.x = (int)touch_lastevent.x;
            t->mouse.y = (int)touch_lastevent.y;
            t->mouse.dx = (int)touch_lastevent.dx;
            t->mouse.dy = (int)touch_lastevent.dy;
            mouse_x = t->mouse.x;
            mouse_y = t->mouse.y;

            t = event_push(EVENT_MOUSE_DOWN);
            t->mouse.button = 1 << 1;
            t->mouse.x = mouse_x;
            t->mouse.y = mouse_y;
            event_push(EVENT_MOUSE_UP);
            t->mouse.button = 1 << 1;
            t->mouse.x = mouse_x;
            t->mouse.y = mouse_y;
            // log_print("long press finished!\n");
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

bool sdlmouse_using_touch()
{
    return using_touch;
}

pt_drv_mouse sdl_mouse = { &sdlmouse_init, &sdlmouse_update, &sdlmouse_shutdown, &sdlmouse_get_x, &sdlmouse_get_y,
    &sdlmouse_is_button_down, &sdlmouse_using_touch };

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

#define PCSPEAKER_RATE 48000
#define PCSPEAKER_1MS 48
#define PCSPEAKER_16K 3
static SDL_AudioStream* beep_output = NULL;
static bool beep_inited = false;
static uint16_t beep_mode = 0;
static bool beep_word = false;
static uint32_t beep_ticks = 0;

void sdlbeep_callback(void* userdata, SDL_AudioStream* stream, int additional_amount, int total_amount)
{
    // Normally the PC speaker is played live by sending commands to the
    // PIT, which creates a plethora of timing problems (especially if you're
    // doing RealSound, which requires doing so at exactly 16kHz).
    // We have the luxury of buffering the commands ahead of time, so we don't need
    // to worry about exact timing. The WoodyPC driver will aggregate 1ms worth
    // of PIT commands, at which point it can be rendered as audio.
    int samples_needed = additional_amount > 0 ? ((additional_amount + PCSPEAKER_1MS * 8) >> 1) : 0;
    int bytes_given = 0;
    if (samples_needed > 0) {
        int16_t samples[PCSPEAKER_1MS];
        while (samples_needed > 0) {
            for (beep_ticks = 0; beep_ticks < PCSPEAKER_1MS; beep_ticks += PCSPEAKER_16K) {
                pcspeaker_sample_update();
                pcspeaker_data_update();
            }
            PCSPEAKER_CallBack(samples, PCSPEAKER_1MS);
            SDL_PutAudioStreamData(stream, samples, sizeof(int16_t) * PCSPEAKER_1MS);
            samples_needed -= PCSPEAKER_1MS;
            bytes_given += sizeof(int16_t) * PCSPEAKER_1MS;
            beep_ticks = 0;
        }
    }
    // printf("sdlbeep_callback: %d additional, %d total, %d given\n", additional_amount, total_amount, bytes_given);
}

void sdlbeep_init()
{
    if (beep_output)
        return;

    if (!audio_out)
        return;

    if (!beep_inited) {
        PCSPEAKER_Init(PCSPEAKER_RATE, sdltimer_ticks);
        beep_inited = true;
    }

    SDL_AudioSpec src = { SDL_AUDIO_S16LE, 1, PCSPEAKER_RATE };
    beep_output = SDL_CreateAudioStream(&src, NULL);
    SDL_SetAudioStreamGetCallback(beep_output, sdlbeep_callback, NULL);
    if (!SDL_BindAudioStream(audio_out, beep_output)) {
        log_print("sdlbeep_init: Failed to bind audio stream to device: %s\n", SDL_GetError());
    }

    pcspeaker_init();
}

void sdlbeep_shutdown()
{
    if (!beep_output)
        return;
    pcspeaker_shutdown();

    PCSPEAKER_ShutDown();
    SDL_DestroyAudioStream(beep_output);
    beep_output = NULL;
    beep_inited = false;
}

void sdlbeep_set_gate(bool enabled)
{
    if (!beep_inited)
        return;

    PCSPEAKER_SetType(enabled ? 3 : 0, (float)beep_ticks / PCSPEAKER_1MS);
}

void sdlbeep_set_mode(uint16_t mode, bool word)
{
    if (!beep_inited)
        return;

    beep_mode = mode;
    beep_word = word;
}

void sdlbeep_set_counter_8(uint8_t counter)
{
    if (!beep_inited)
        return;

    PCSPEAKER_SetCounter(counter, beep_mode, (float)beep_ticks / PCSPEAKER_1MS);
}

void sdlbeep_set_counter_16(uint16_t counter)
{
    if (!beep_inited)
        return;

    PCSPEAKER_SetCounter(counter, beep_mode, (float)beep_ticks / PCSPEAKER_1MS);
}

pt_drv_beep sdl_beep = { &sdlbeep_init, &sdlbeep_shutdown, &sdlbeep_set_gate, &sdlbeep_set_mode, &sdlbeep_set_counter_8,
    &sdlbeep_set_counter_16 };

// For some reason you can't actually include woodyopl/opl.h, as it has a bunch of
// player state embedded into it. Oh well!
extern void adlib_init(uint32_t samplerate);
extern void adlib_write(uintptr_t idx, uint8_t val);
extern void adlib_getsample(int16_t* sndptr, intptr_t numsamples);

#ifdef __EMSCRIPTEN__
// Safari doesn't like weird sample rates
#define OPL_RATE 48000
#else
#define OPL_RATE 49716
#endif
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
