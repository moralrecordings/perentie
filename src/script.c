#include <stdlib.h>
#include <string.h>

#include "lua/lua.h"
#include "lua/lauxlib.h"
#include "lua/lualib.h"

#include "dos.h"
#include "font.h"
#include "image.h"
#include "log.h"
#include "text.h"
#include "version.h"


lua_State *main_thread = NULL;

inline const char *lua_strcpy(lua_State *L, int index, size_t *size) {
    size_t n;
    const char *name_src = lua_tolstring(L, index, &n);
    char *dest = (char *)calloc(n+1, sizeof(char));
    memcpy(dest, name_src, n);
    if (size)
        *size = n;
    return dest;
}

static int lua_pt_version(lua_State *L) {
    lua_pushstring(L, VERSION);
    return 1;
}

static int lua_pt_get_millis(lua_State *L) {
    lua_pushinteger(L, timer_millis());
    return 1;
}

static int lua_pt_play_beep(lua_State *L) {
    float freq = luaL_checknumber(L, 1);
    pcspeaker_tone(freq);
    return 0;
}

static int lua_pt_stop_beep(lua_State *L) {
    pcspeaker_stop();
    return 0;
}

static int lua_pt_image_gc(lua_State *L) {
    pt_image **target = (pt_image **)lua_touserdata(L, 1);
    if (target && *target) {
        destroy_image(*target);
        *target = NULL;
    }
    return 0;
}

static int lua_pt_image(lua_State *L) {
    const char *path = lua_strcpy(L, 1, NULL);
    int16_t origin_x = 0;
    int16_t origin_y = 0;
    if (lua_gettop(L) == 3) {
        origin_x = luaL_checkinteger(L, 2);
        origin_y = luaL_checkinteger(L, 3);
    }

    pt_image *image = create_image(path, origin_x, origin_y);
    log_print("lua_pt_create_image: %p %d %d\n", image, image->width, image->height);
    // create a table
    pt_image **target = lua_newuserdatauv(L, sizeof(pt_image *), 1);
    *target = image;
    lua_newtable(L);
    lua_pushstring(L, "PTImage");
    lua_setfield(L, -2, "__name");
    lua_pushcfunction(L, lua_pt_image_gc);
    lua_setfield(L, -2, "__gc");
    lua_setmetatable(L, -1);
    return 1;
}

static int lua_pt_font_gc(lua_State *L) {
    pt_font **target = (pt_font **)lua_touserdata(L, 1);
    if (target && *target) {
        destroy_font(*target);
        *target = NULL;
    }
    return 0;
}

static int lua_pt_font(lua_State *L) {
    const char *path = lua_strcpy(L, 1, NULL);
    pt_font *font = create_font(path);
    pt_font **target = lua_newuserdatauv(L, sizeof(pt_font *), 1);
    *target = font;
    lua_newtable(L);
    lua_pushstring(L, "PTFont");
    lua_setfield(L, -2, "__name");
    lua_pushcfunction(L, lua_pt_font_gc);
    lua_setfield(L, -2, "__gc");
    lua_setmetatable(L, -1);
    return 1;
}

static int lua_pt_text(lua_State *L) {
    size_t len = 0;
    const byte *string = (const byte *)luaL_checklstring(L, 1, &len);
    pt_font **fontptr = (pt_font **)lua_touserdata(L, 2);
    uint16_t width = luaL_checkinteger(L, 3);
    enum pt_text_align align = (enum pt_text_align)luaL_checkinteger(L, 4);
    uint8_t r = (uint8_t)luaL_checkinteger(L, 5);
    uint8_t g = (uint8_t)luaL_checkinteger(L, 6);
    uint8_t b = (uint8_t)luaL_checkinteger(L, 7);

    pt_text *text = create_text(string, len, *fontptr, width, ALIGN_CENTER);
    pt_image *image = text_to_image(text, r, g, b);
    destroy_text(text);

    // same as the lua_pt_image code
    pt_image **target = lua_newuserdatauv(L, sizeof(pt_image *), 1);
    *target = image;
    lua_newtable(L);
    lua_pushstring(L, "PTImage");
    lua_setfield(L, -2, "__name");
    lua_pushcfunction(L, lua_pt_image_gc);
    lua_setfield(L, -2, "__gc");
    lua_setmetatable(L, -1);
    return 1;
}

static int lua_pt_clear_screen(lua_State *L) { 
    video_clear();
    return 0;
}

static int lua_pt_draw_image(lua_State *L) {
    //pt_image **imageptr = (pt_image **)luaL_checkudata(L, 1, "PTImage");
    pt_image **imageptr = (pt_image **)lua_touserdata(L, 1);
    int16_t x = luaL_checkinteger(L, 2);
    int16_t y = luaL_checkinteger(L, 3);
    //log_print("lua_pt_draw_image: %p %d %d\n", *imageptr, x, y);
    video_blit_image(*imageptr, x, y);
    return 0;
}

static int lua_pt_log(lua_State *L) {
    const char *line = luaL_checkstring(L, 1);
    log_print("%s\n", line);
    return 0;
}

static int lua_pt_get_mouse_pos(lua_State *L) {
    lua_pushinteger(L, mouse_get_x()); 
    lua_pushinteger(L, mouse_get_y()); 
    return 2;
};

static const struct luaL_Reg lua_funcs [] = {
    {"_PTVersion", lua_pt_version},
    {"_PTGetMillis", lua_pt_get_millis},
    {"_PTPlayBeep", lua_pt_play_beep},
    {"_PTStopBeep", lua_pt_stop_beep},
    {"_PTImage", lua_pt_image},
    {"_PTFont", lua_pt_font},
    {"_PTText", lua_pt_text},
    {"_PTClearScreen", lua_pt_clear_screen},
    {"_PTDrawImage", lua_pt_draw_image},
    {"_PTLog", lua_pt_log},
    {"_PTGetMousePos", lua_pt_get_mouse_pos},
    {NULL, NULL},
};

int script_exec() {
    if (!main_thread)
        return 0;
    lua_getglobal(main_thread, "_PTRunThreads");
    lua_call(main_thread, 0, 1);
    int result = (int)lua_tointeger(main_thread, 1);
    lua_pop(main_thread, 1);
    return result;
}

void script_draw() {
    lua_getglobal(main_thread, "_PTRender");
    lua_call(main_thread, 0, 0);
}

void script_init() {
    if (main_thread) {
        log_print("script_init(): already started!");
        return;
    }
    main_thread = luaL_newstate();
    // load in standard libraries
    luaL_openlibs(main_thread);
    // add our C bindings
    const luaL_Reg *funcs_ptr = lua_funcs; 
    while (funcs_ptr->name) {
        lua_register(main_thread, funcs_ptr->name, funcs_ptr->func);
        funcs_ptr++;
    }
    if (luaL_dofile(main_thread, "boot.lua") != LUA_OK) {
        log_print("script_init(): boot.lua: %s\n", lua_tostring(main_thread, -1));
        luaL_traceback(main_thread, main_thread, NULL, 1);
        log_print("%s", lua_tostring(main_thread, 1));
        lua_pop(main_thread, 1);
        exit(1);
    }
    if (luaL_dofile(main_thread, "test.lua") != LUA_OK) {
        log_print("script_init(): test.lua: %s\n", lua_tostring(main_thread, -1));
        luaL_traceback(main_thread, main_thread, NULL, 1);
        log_print("%s", lua_tostring(main_thread, 1));
        lua_pop(main_thread, 1);
        exit(1);
    }
}

