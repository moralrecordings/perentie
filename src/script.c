#include <stdlib.h>
#include <string.h>

#include "lua/lua.h"
#include "lua/lauxlib.h"
#include "lua/lualib.h"

#include "dos.h"
#include "image.h"
#include "version.h"


lua_State *main_thread = NULL;

inline const char *lua_strcpy(lua_State *L, int index, size_t *size) {
    size_t n;
    const char *name_src = lua_tolstring(L, index, &n);
    char *dest = (char *)calloc(n, sizeof(char));
    memcpy(dest, name_src, n*sizeof(char));
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
    pt_image *image = create_image(path);
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

static int lua_pt_clear_screen(lua_State *L) {
    video_clear();
    return 0;
}

static int lua_pt_draw_image(lua_State *L) {
    pt_image **imageptr = (pt_image **)luaL_checkudata(L, 1, "PTImage");
    uint16_t x = luaL_checkinteger(L, 2);
    uint16_t y = luaL_checkinteger(L, 3);
    video_blit_image(*imageptr, x, y);
    return 0;
}


static const struct luaL_Reg lua_funcs [] = {
    {"_PTVersion", lua_pt_version},
    {"_PTGetMillis", lua_pt_get_millis},
    {"_PTPlayBeep", lua_pt_play_beep},
    {"_PTStopBeep", lua_pt_stop_beep},
    {"_PTImage", lua_pt_image},
    {"_PTClearScreen", lua_pt_clear_screen},
    {"_PTDrawImage", lua_pt_draw_image},
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
        printf("script_init(): already started!");
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
        printf("script_init(): boot.lua: %s\n", lua_tostring(main_thread, -1));
        luaL_traceback(main_thread, main_thread, NULL, 1);
        printf("%s", lua_tostring(main_thread, 1));
        lua_pop(main_thread, 1);
        exit(1);
    }
    if (luaL_dofile(main_thread, "test.lua") != LUA_OK) {
        printf("script_init(): test.lua: %s\n", lua_tostring(main_thread, -1));
        luaL_traceback(main_thread, main_thread, NULL, 1);
        printf("%s", lua_tostring(main_thread, 1));
        lua_pop(main_thread, 1);
        exit(1);
    }
}

