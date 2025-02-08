#include <stdlib.h>
#include <string.h>

#include "lua/lauxlib.h"
#include "lua/lua.h"
#include "lua/lualib.h"

#include "event.h"
#include "font.h"
#include "image.h"
#include "log.h"
#include "musicrad.h"
#include "repl.h"
#include "script.h"
#include "system.h"
#include "text.h"
#include "version.h"

lua_State* main_thread = NULL;
static bool has_quit = false;
static bool has_reset = false;
static char* reset_state_path = NULL;
static int quit_status = 0;
static char* crash_message = NULL;

bool script_has_quit()
{
    return has_quit;
}

int script_quit_status()
{
    return quit_status;
}

char* script_crash_message()
{
    return crash_message;
}

char* lua_strcpy(lua_State* L, int index, size_t* size)
{
    size_t n = 0;
    const char* name_src = lua_tolstring(L, index, &n);
    if (!name_src) {
        if (size)
            size = 0;
        return NULL;
    }
    char* dest = (char*)calloc(n + 1, sizeof(char));
    memcpy(dest, name_src, n);
    if (size)
        *size = n;
    return dest;
}

static int lua_pt_version(lua_State* L)
{
    lua_pushstring(L, VERSION);
    return 1;
}

static int lua_pt_get_millis(lua_State* L)
{
    lua_pushinteger(L, pt_sys.timer->millis());
    return 1;
}

static int lua_pt_play_beep(lua_State* L)
{
    float freq = luaL_checknumber(L, 1);
    pt_sys.beep->tone(freq);
    return 0;
}

static int lua_pt_stop_beep(lua_State* L)
{
    pt_sys.beep->stop();
    return 0;
}

static int lua_pt_rad_load(lua_State* L)
{
    const char* path = lua_strcpy(L, 1, NULL);
    lua_pushboolean(L, radplayer_load_file(path));
    return 1;
}

static int lua_pt_rad_play(lua_State* L)
{
    radplayer_play();
    return 0;
}

static int lua_pt_rad_stop(lua_State* L)
{
    radplayer_stop();
    return 0;
}

static int lua_pt_rad_get_volume(lua_State* L)
{
    lua_pushinteger(L, radplayer_get_master_volume());
    return 1;
}

static int lua_pt_rad_set_volume(lua_State* L)
{
    int vol = luaL_checkinteger(L, 1);
    radplayer_set_master_volume(vol);
    return 0;
}

static int lua_pt_rad_get_position(lua_State* L)
{
    lua_pushinteger(L, radplayer_get_order());
    lua_pushinteger(L, radplayer_get_line());
    return 2;
}

static int lua_pt_rad_set_position(lua_State* L)
{
    int order = luaL_checkinteger(L, 1);
    int line = luaL_checkinteger(L, 2);
    radplayer_set_position(order, line);
    return 0;
}

static int lua_pt_image_gc(lua_State* L)
{
    pt_image** target = (pt_image**)lua_touserdata(L, 1);
    if (target && *target) {
        destroy_image(*target);
        *target = NULL;
    }
    return 0;
}

static int lua_pt_image(lua_State* L)
{
    const char* path = lua_strcpy(L, 1, NULL);
    int16_t origin_x = 0;
    int16_t origin_y = 0;
    int16_t colourkey = -1;
    if (lua_gettop(L) >= 3) {
        origin_x = luaL_checkinteger(L, 2);
        origin_y = luaL_checkinteger(L, 3);
    }
    if (lua_gettop(L) == 4) {
        colourkey = luaL_checkinteger(L, 4);
    }

    pt_image* image = create_image(path, origin_x, origin_y, colourkey);
    log_print("lua_pt_create_image: %p %d %d %d\n", image, image->width, image->height, image->colourkey);
    // create a table
    pt_image** target = lua_newuserdatauv(L, sizeof(pt_image*), 1);
    *target = image;
    lua_newtable(L);
    lua_pushstring(L, "PTImage");
    lua_setfield(L, -2, "__name");
    lua_pushcfunction(L, lua_pt_image_gc);
    lua_setfield(L, -2, "__gc");
    lua_setmetatable(L, -1);
    return 1;
}

static int lua_pt_get_image_dims(lua_State* L)
{
    pt_image** imageptr = (pt_image**)lua_touserdata(L, 1);
    if (!imageptr) {
        log_print("lua_pt_get_image_dims: invalid or missing image pointer\n");
        lua_pushinteger(L, 0);
        lua_pushinteger(L, 0);
        return 2;
    }
    lua_pushinteger(L, (*imageptr)->width);
    lua_pushinteger(L, (*imageptr)->height);
    return 2;
}

static int lua_pt_get_image_origin(lua_State* L)
{
    pt_image** imageptr = (pt_image**)lua_touserdata(L, 1);
    if (!imageptr) {
        log_print("lua_pt_get_image_origin: invalid or missing image pointer\n");
        lua_pushinteger(L, 0);
        lua_pushinteger(L, 0);
        return 2;
    }
    lua_pushinteger(L, (*imageptr)->origin_x);
    lua_pushinteger(L, (*imageptr)->origin_y);
    return 2;
}

static int lua_pt_set_image_origin(lua_State* L)
{
    pt_image** imageptr = (pt_image**)lua_touserdata(L, 1);
    if (!imageptr) {
        log_print("lua_pt_get_image_origin: invalid or missing image pointer\n");
        return 0;
    }
    (*imageptr)->origin_x = (int16_t)luaL_checkinteger(L, 2);
    (*imageptr)->origin_y = (int16_t)luaL_checkinteger(L, 3);
    log_print("lua_pt_set_image_origin: setting %p origin to %d, %d\n", (*imageptr), (*imageptr)->origin_x,
        (*imageptr)->origin_y);
    return 0;
}

static int lua_pt_font_gc(lua_State* L)
{
    pt_font** target = (pt_font**)lua_touserdata(L, 1);
    if (target && *target) {
        destroy_font(*target);
        *target = NULL;
    }
    return 0;
}

static int lua_pt_font(lua_State* L)
{
    const char* path = lua_strcpy(L, 1, NULL);
    pt_font* font = create_font(path);
    if (!font) {
        lua_pushnil(L);
        return 1;
    }
    pt_font** target = lua_newuserdatauv(L, sizeof(pt_font*), 1);
    *target = font;
    lua_newtable(L);
    lua_pushstring(L, "PTFont");
    lua_setfield(L, -2, "__name");
    lua_pushcfunction(L, lua_pt_font_gc);
    lua_setfield(L, -2, "__gc");
    lua_setmetatable(L, -1);
    return 1;
}

static int lua_pt_text(lua_State* L)
{
    size_t len = 0;
    const byte* string = (const byte*)luaL_checklstring(L, 1, &len);
    pt_font** fontptr = (pt_font**)lua_touserdata(L, 2);
    if (!fontptr) {
        log_print("lua_pt_text: invalid or missing font\n");
        lua_pushnil(L);
        return 1;
    }
    uint16_t width = luaL_checkinteger(L, 3);
    enum pt_text_align align = (enum pt_text_align)luaL_checkinteger(L, 4);
    uint8_t r = (uint8_t)luaL_checkinteger(L, 5);
    uint8_t g = (uint8_t)luaL_checkinteger(L, 6);
    uint8_t b = (uint8_t)luaL_checkinteger(L, 7);

    pt_text* text = create_text(string, len, *fontptr, width, align);
    pt_image* image = text_to_image(text, r, g, b);
    destroy_text(text);

    // same as the lua_pt_image code
    pt_image** target = lua_newuserdatauv(L, sizeof(pt_image*), 1);
    *target = image;
    lua_newtable(L);
    lua_pushstring(L, "PTImage");
    lua_setfield(L, -2, "__name");
    lua_pushcfunction(L, lua_pt_image_gc);
    lua_setfield(L, -2, "__gc");
    lua_setmetatable(L, -1);
    return 1;
}

static int lua_pt_clear_screen(lua_State* L)
{
    pt_sys.video->clear();
    return 0;
}

static int lua_pt_draw_image(lua_State* L)
{
    // pt_image **imageptr = (pt_image **)luaL_checkudata(L, 1, "PTImage");
    pt_image** imageptr = (pt_image**)lua_touserdata(L, 1);
    if (!imageptr) {
        log_print("lua_pt_draw_image: invalid or missing image pointer\n");
        return 0;
    }
    int16_t x = luaL_checkinteger(L, 2);
    int16_t y = luaL_checkinteger(L, 3);
    uint8_t flags = luaL_checkinteger(L, 4);
    // log_print("lua_pt_draw_image: %p %d %d\n", *imageptr, x, y);
    image_blit(*imageptr, x, y, flags);
    return 0;
}

static int lua_pt_draw_9slice(lua_State* L)
{
    // pt_image **imageptr = (pt_image **)luaL_checkudata(L, 1, "PTImage");
    pt_image** imageptr = (pt_image**)lua_touserdata(L, 1);
    if (!imageptr) {
        log_print("lua_pt_draw_9slice: invalid or missing image pointer\n");
        return 0;
    }
    int16_t x = luaL_checkinteger(L, 2);
    int16_t y = luaL_checkinteger(L, 3);
    uint8_t flags = luaL_checkinteger(L, 4);
    uint16_t width = luaL_checkinteger(L, 5);
    uint16_t height = luaL_checkinteger(L, 6);
    int16_t x1 = luaL_checkinteger(L, 7);
    int16_t y1 = luaL_checkinteger(L, 8);
    int16_t x2 = luaL_checkinteger(L, 9);
    int16_t y2 = luaL_checkinteger(L, 10);
    // log_print("lua_pt_draw_image: %p %d %d\n", *imageptr, x, y);
    image_blit_9slice(*imageptr, x, y, flags, width, height, x1, y1, x2, y2);
    return 0;
}

static int lua_pt_draw_line(lua_State* L)
{
    int16_t x0 = luaL_checkinteger(L, 1);
    int16_t y0 = luaL_checkinteger(L, 2);
    int16_t x1 = luaL_checkinteger(L, 3);
    int16_t y1 = luaL_checkinteger(L, 4);
    pt_colour_rgb colour = { 0 };
    colour.r = luaL_checkinteger(L, 5);
    colour.g = luaL_checkinteger(L, 6);
    colour.b = luaL_checkinteger(L, 7);
    pt_sys.video->blit_line(x0, y0, x1, y1, &colour);
    return 0;
}

static int lua_pt_image_test_collision(lua_State* L)
{
    pt_image** imageptr = (pt_image**)lua_touserdata(L, 1);
    if (!imageptr) {
        log_print("lua_pt_image_test_collision: invalid or missing image pointer\n");
        lua_pushboolean(L, false);
        return 1;
    }
    int16_t x = luaL_checkinteger(L, 2);
    int16_t y = luaL_checkinteger(L, 3);
    uint8_t flags = luaL_checkinteger(L, 4);
    bool mask = lua_toboolean(L, 5);
    lua_pushboolean(L, image_test_collision(*imageptr, x, y, mask, flags));
    return 1;
}

static int lua_pt_9slice_test_collision(lua_State* L)
{
    pt_image** imageptr = (pt_image**)lua_touserdata(L, 1);
    if (!imageptr) {
        log_print("lua_pt_image_test_collision: invalid or missing image pointer\n");
        lua_pushboolean(L, false);
        return 1;
    }
    int16_t x = luaL_checkinteger(L, 2);
    int16_t y = luaL_checkinteger(L, 3);
    uint8_t flags = luaL_checkinteger(L, 4);
    bool mask = lua_toboolean(L, 5);
    uint16_t width = luaL_checkinteger(L, 6);
    uint16_t height = luaL_checkinteger(L, 7);
    int16_t x1 = luaL_checkinteger(L, 8);
    int16_t y1 = luaL_checkinteger(L, 9);
    int16_t x2 = luaL_checkinteger(L, 10);
    int16_t y2 = luaL_checkinteger(L, 11);
    lua_pushboolean(L, image_test_collision_9slice(*imageptr, x, y, mask, flags, width, height, x1, y1, x2, y2));
    return 1;
}

static int lua_pt_log(lua_State* L)
{
    const char* line = luaL_checkstring(L, 1);
    log_print("%s\n", line);
    return 0;
}

static int lua_pt_pump_event(lua_State* L)
{
    pt_event* ev = event_pop();
    if (!ev) {
        lua_pushnil(L);
        return 1;
    }
    lua_createtable(L, 0, 6);
    lua_pushstring(L, "PTEvent");
    lua_setfield(L, -2, "_type");
    switch (ev->type) {
    case EVENT_NULL:
        lua_pushstring(L, "null");
        lua_setfield(L, -2, "type");
        break;
    case EVENT_QUIT:
        lua_pushstring(L, "quit");
        lua_setfield(L, -2, "type");
        lua_pushinteger(L, ev->quit.status);
        lua_setfield(L, -2, "status");
        // if something's raised the quit event,
        // quit the engine after the current loop
        has_quit = true;
        quit_status = ev->quit.status;
        break;
    case EVENT_RESET:
        lua_pushstring(L, "reset");
        lua_setfield(L, -2, "type");
        if (ev->reset.state_path) {
            lua_pushstring(L, ev->reset.state_path);
        } else {
            lua_pushnil(L);
        }
        lua_setfield(L, -2, "statePath");
        // if something's raised the reset event,
        // tell the engine to reload everything
        // after the current loop
        has_reset = true;
        reset_state_path = ev->reset.state_path;
        ev->reset.state_path = NULL;
        break;
    case EVENT_KEY_DOWN:
        lua_pushstring(L, "keyDown");
        lua_setfield(L, -2, "type");
        lua_pushstring(L, ev->keyboard.key);
        lua_setfield(L, -2, "key");
        lua_pushboolean(L, ev->keyboard.isrepeat);
        lua_setfield(L, -2, "isRepeat");
        lua_pushinteger(L, ev->keyboard.flags);
        lua_setfield(L, -2, "flags");
        break;
    case EVENT_KEY_UP:
        lua_pushstring(L, "keyUp");
        lua_setfield(L, -2, "type");
        lua_pushstring(L, ev->keyboard.key);
        lua_setfield(L, -2, "key");
        break;
    case EVENT_MOUSE_MOVE:
        lua_pushstring(L, "mouseMove");
        lua_setfield(L, -2, "type");
        lua_pushinteger(L, ev->mouse.x);
        lua_setfield(L, -2, "x");
        lua_pushinteger(L, ev->mouse.y);
        lua_setfield(L, -2, "y");
        lua_pushinteger(L, ev->mouse.dx);
        lua_setfield(L, -2, "dx");
        lua_pushinteger(L, ev->mouse.dy);
        lua_setfield(L, -2, "dy");
        break;
    case EVENT_MOUSE_DOWN:
        lua_pushstring(L, "mouseDown");
        lua_setfield(L, -2, "type");
        lua_pushinteger(L, ev->mouse.x);
        lua_setfield(L, -2, "x");
        lua_pushinteger(L, ev->mouse.y);
        lua_setfield(L, -2, "y");
        lua_pushinteger(L, ev->mouse.button);
        lua_setfield(L, -2, "button");
        break;
    case EVENT_MOUSE_UP:
        lua_pushstring(L, "mouseUp");
        lua_setfield(L, -2, "type");
        lua_pushinteger(L, ev->mouse.x);
        lua_setfield(L, -2, "x");
        lua_pushinteger(L, ev->mouse.y);
        lua_setfield(L, -2, "y");
        lua_pushinteger(L, ev->mouse.button);
        lua_setfield(L, -2, "button");
        break;
    default:
        break;
    }
    return 1;
}

static int lua_pt_get_mouse_pos(lua_State* L)
{
    lua_pushinteger(L, pt_sys.mouse->get_x());
    lua_pushinteger(L, pt_sys.mouse->get_y());
    return 2;
};

static int lua_pt_get_palette(lua_State* L)
{
    lua_createtable(L, 3 * 256, 0);
    for (int i = 0; i < pt_sys.palette_top; i++) {
        lua_pushinteger(L, pt_sys.palette[i].r);
        lua_seti(L, -2, 3 * i + 1);
        lua_pushinteger(L, pt_sys.palette[i].g);
        lua_seti(L, -2, 3 * i + 2);
        lua_pushinteger(L, pt_sys.palette[i].b);
        lua_seti(L, -2, 3 * i + 3);
    }
    return 1;
};

static int lua_pt_set_palette_remapper(lua_State* L)
{
    enum pt_palette_remapper remapper = luaL_checkinteger(L, 1);
    pt_sys.video->set_palette_remapper(remapper);
    return 0;
};

static int lua_pt_set_dither_hint(lua_State* L)
{
    pt_colour_rgb src = { 0 };
    lua_geti(L, 1, 1);
    src.r = luaL_checkinteger(L, -1);
    lua_pop(L, 1);
    lua_geti(L, 1, 2);
    src.g = luaL_checkinteger(L, -1);
    lua_pop(L, 1);
    lua_geti(L, 1, 3);
    src.b = luaL_checkinteger(L, -1);
    lua_pop(L, 1);

    enum pt_dither_type type = luaL_checkinteger(L, 2);

    pt_colour_rgb a = { 0 };
    pt_colour_rgb b = { 0 };
    lua_geti(L, 3, 1);
    a.r = luaL_checkinteger(L, -1);
    lua_pop(L, 1);
    lua_geti(L, 3, 2);
    a.g = luaL_checkinteger(L, -1);
    lua_pop(L, 1);
    lua_geti(L, 3, 3);
    a.b = luaL_checkinteger(L, -1);
    lua_pop(L, 1);
    lua_geti(L, 3, 4);
    b.r = luaL_checkinteger(L, -1);
    lua_pop(L, 1);
    lua_geti(L, 3, 5);
    b.g = luaL_checkinteger(L, -1);
    lua_pop(L, 1);
    lua_geti(L, 3, 6);
    b.b = luaL_checkinteger(L, -1);
    lua_pop(L, 1);

    pt_sys.video->set_dither_hint(&src, type, &a, &b);
    return 0;
};

static int lua_pt_set_debug_console(lua_State* L)
{
    bool enabled = lua_toboolean(L, 1);
    const char* device = luaL_checkstring(L, 2);
    if (enabled) {
        pt_sys.serial->open_device(device);
    } else {
        pt_sys.serial->close_device();
    }
    return 0;
}

static int lua_pt_reset(lua_State* L)
{
    pt_event* ev = event_push(EVENT_RESET);
    size_t path_size = 0;
    char* path = lua_strcpy(L, 1, &path_size);
    ev->reset.state_path = path;
    return 0;
};

static int lua_pt_quit(lua_State* L)
{
    pt_event* ev = event_push(EVENT_QUIT);
    ev->quit.status = luaL_checkinteger(L, 1);
    return 0;
};

static const struct luaL_Reg lua_funcs[] = {
    { "_PTVersion", lua_pt_version },
    { "_PTGetMillis", lua_pt_get_millis },
    { "_PTPlayBeep", lua_pt_play_beep },
    { "_PTStopBeep", lua_pt_stop_beep },
    { "_PTRadLoad", lua_pt_rad_load },
    { "_PTRadPlay", lua_pt_rad_play },
    { "_PTRadStop", lua_pt_rad_stop },
    { "_PTRadGetVolume", lua_pt_rad_get_volume },
    { "_PTRadSetVolume", lua_pt_rad_set_volume },
    { "_PTRadGetPosition", lua_pt_rad_get_position },
    { "_PTRadSetPosition", lua_pt_rad_set_position },
    { "_PTImage", lua_pt_image },
    { "_PTGetImageDims", lua_pt_get_image_dims },
    { "_PTGetImageOrigin", lua_pt_get_image_origin },
    { "_PTSetImageOrigin", lua_pt_set_image_origin },
    { "_PTFont", lua_pt_font },
    { "_PTText", lua_pt_text },
    { "_PTClearScreen", lua_pt_clear_screen },
    { "_PTDrawImage", lua_pt_draw_image },
    { "_PTDraw9Slice", lua_pt_draw_9slice },
    { "_PTDrawLine", lua_pt_draw_line },
    { "_PTImageTestCollision", lua_pt_image_test_collision },
    { "_PT9SliceTestCollision", lua_pt_9slice_test_collision },
    { "_PTLog", lua_pt_log },
    { "_PTPumpEvent", lua_pt_pump_event },
    { "_PTGetMousePos", lua_pt_get_mouse_pos },
    { "_PTGetPalette", lua_pt_get_palette },
    { "_PTSetPaletteRemapper", lua_pt_set_palette_remapper },
    { "_PTSetDitherHint", lua_pt_set_dither_hint },
    { "_PTSetDebugConsole", lua_pt_set_debug_console },
    { "_PTReset", lua_pt_reset },
    { "_PTQuit", lua_pt_quit },
    { NULL, NULL },
};

int script_exec()
{
    if (has_reset)
        script_reset();
    if (!main_thread)
        return 0;
    lua_getglobal(main_thread, "_PTRunThreads");
    lua_call(main_thread, 0, 1);
    int result = (int)lua_tointeger(main_thread, 1);
    lua_pop(main_thread, 1);
    return result;
}

void script_events()
{
    lua_getglobal(main_thread, "_PTEvents");
    if (lua_pcall(main_thread, 0, 0, 0) != LUA_OK) {
        const char* error = lua_tostring(main_thread, -1);
        log_print("script_events(): error: %s\n", error);
        lua_pop(main_thread, 1);
    }
}

void script_repl()
{
    repl_update(main_thread);
}

void script_render()
{
    lua_getglobal(main_thread, "_PTRender");
    if (lua_pcall(main_thread, 0, 0, 0) != LUA_OK) {
        const char* error = lua_tostring(main_thread, -1);
        log_print("script_render(): error: %s\n", error);
        lua_pop(main_thread, 1);
    }
}

void script_init()
{
    if (main_thread) {
        log_print("script_init(): already started!");
        return;
    }
    main_thread = luaL_newstate();
    // load in standard libraries
    luaL_openlibs(main_thread);
    // add our C bindings
    const luaL_Reg* funcs_ptr = lua_funcs;
    while (funcs_ptr->name) {
        lua_register(main_thread, funcs_ptr->name, funcs_ptr->func);
        funcs_ptr++;
    }
    int result;
    // load inspect module
#include "inspect.h"
    if (result != LUA_OK) {
        log_print("script_init(): inspect: %s\n", lua_tostring(main_thread, -1));
        luaL_traceback(main_thread, main_thread, NULL, 1);
        log_print("%s", lua_tostring(main_thread, 1));
        lua_pop(main_thread, 1);
        exit(1);
    }
    lua_setglobal(main_thread, "inspect");

    // load cbor module
#include "cbor.h"
    if (result != LUA_OK) {
        log_print("script_init(): cbor: %s\n", lua_tostring(main_thread, -1));
        luaL_traceback(main_thread, main_thread, NULL, 1);
        log_print("%s", lua_tostring(main_thread, 1));
        lua_pop(main_thread, 1);
        exit(1);
    }
    lua_setglobal(main_thread, "cbor");

    // load perentie Lua code
#include "boot.h"
    if (result != LUA_OK) {
        log_print("script_init(): boot: %s\n", lua_tostring(main_thread, -1));
        luaL_traceback(main_thread, main_thread, NULL, 1);
        log_print("%s", lua_tostring(main_thread, 1));
        lua_pop(main_thread, 1);
        exit(1);
    }

    // load in target game's lua code
    lua_getglobal(main_thread, "_PTWhoops");
    int init_result = luaL_loadfile(main_thread, "main.lua") || lua_pcall(main_thread, 0, LUA_MULTRET, 1);
    if (init_result != LUA_OK) {
        crash_message = lua_strcpy(main_thread, -1, NULL);
        log_print("%s", crash_message);
        lua_pop(main_thread, 1);
        exit(1);
    }
    lua_pop(main_thread, 1);
    repl_init(main_thread);
}

void script_reset()
{
    log_print("script_reset(): Resetting Perentie state!\n");
    lua_close(main_thread);
    main_thread = NULL;
    script_init();
    has_reset = false;
    if (reset_state_path) {
        log_print("script_reset(): Loading state from path %s\n", reset_state_path);
        lua_getglobal(main_thread, "_PTInitFromStateFile");
        lua_pushstring(main_thread, reset_state_path);
        if (lua_pcall(main_thread, 1, 0, 0) != LUA_OK) {
            const char* error = lua_tostring(main_thread, -1);
            log_print("script_reset(): error: %s\n", error);
            lua_pop(main_thread, 1);
        }
        free(reset_state_path);
        reset_state_path = NULL;
    }
}
