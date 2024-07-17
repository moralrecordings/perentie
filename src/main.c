#include <stdio.h>
#include <stdlib.h>
#include "lua/lua.h"
#include "lua/lauxlib.h"
#include "lua/lualib.h"

int main(int argc, char **argv) {
    lua_State *L = luaL_newstate();
    luaL_openlibs(L);
    luaL_dofile(L, "test.lua");

    return 0;
}
