#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include "lua/lua.h"
#include "lua/lauxlib.h"
#include "lua/lualib.h"

#include "dos.h"

int main(int argc, char **argv) {
    lua_State *L = luaL_newstate();
    luaL_openlibs(L);

    video_init();
    for (int i = 0; i < 10; i++) {
        video_test(i);
        usleep(500000);
    }
    luaL_dofile(L, "test.lua");
    return 0;
}
