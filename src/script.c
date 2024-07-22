#include "lua/lua.h"
#include "lua/lauxlib.h"
#include "lua/lualib.h"
#include "stb/stb_ds.h"

#include "dos.h"


lua_State *main_thread = NULL;

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

static const struct luaL_Reg lua_funcs [] = {
    {"_PTGetMillis", lua_pt_get_millis},
    {"_PTPlayBeep", lua_pt_play_beep},
    {"_PTStopBeep", lua_pt_stop_beep},
    {NULL, NULL},
};

int script_exec() {
    lua_getglobal(main_thread, "_PTRunThreads");
    lua_call(main_thread, 0, 1);
    int result = (int)lua_tointeger(main_thread, 1);
    lua_pop(main_thread, 1);
    return result;
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
        printf("script_init(): - %s", lua_tostring(main_thread, -1));
        luaL_traceback(main_thread, main_thread, NULL, 1);
        printf("%s", lua_tostring(main_thread, 1));
        lua_pop(main_thread, 1);
        exit(1);
    }
    if (luaL_dofile(main_thread, "test.lua") != LUA_OK) {
        printf("script_init(): - %s", lua_tostring(main_thread, -1));
        luaL_traceback(main_thread, main_thread, NULL, 1);
        printf("%s", lua_tostring(main_thread, 1));
        lua_pop(main_thread, 1);
        exit(1);
    }
}

