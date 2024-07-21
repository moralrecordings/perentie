#include "lua/lua.h"
#include "lua/lauxlib.h"
#include "lua/lualib.h"
#include "stb/stb_ds.h"

#include "dos.h"


struct script_thread {
    int thread_id;
    lua_State *state;
    uint32_t sleep_until;
    int ref;
};

lua_State *main_thread = NULL;
struct script_thread **threadlist = NULL;

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

static int lua_pt_sleep(lua_State *L) {
    int delay = luaL_checkinteger(L, 1);
    for (int i = 0; i < arrlen(threadlist); i++) {
        if (threadlist[i]->state == L) {
            threadlist[i]->sleep_until = timer_millis() + delay;
            return 0;
        }
    }
    luaL_argerror(L, 1, "thread not found");
    return 0;
}

static int lua_pt_start_thread(lua_State *L) {
    int thread_id = luaL_checkinteger(L, 1);
    for (int i = 0; i < arrlen(threadlist); i++) {
        if (threadlist[i]->thread_id == thread_id) {
            luaL_argerror(L, 1, "thread ID exists");
            return 0;
        }
    }
    if (!lua_isfunction(L, 2)) {
        luaL_argerror(L, 2, "must be a function");
        return 0;
    }

    struct script_thread *st = (struct script_thread *)calloc(1, sizeof(struct script_thread));

    // make a new lua thread
    st->state = lua_newthread(L);
    st->thread_id = thread_id;
    // grab the function from the stack and put it in the thread's stack
    lua_pushvalue(L, 2);
    lua_xmove(L, st->state, 1);
    // increase thread's reference count
    lua_pushthread(st->state);
    st->ref = luaL_ref(st->state, 1);

    // shove it into the thread lookup table
    arrpush(threadlist, st);
    return 0;
}

static int lua_pt_stop_thread(lua_State *L) {
    int thread_id = luaL_checkinteger(L, 1);
    struct script_thread *target = NULL;
    int index = 0; 
    for ( ; index < arrlen(threadlist); index++) {
        if (threadlist[index]->thread_id == thread_id) {
            target = threadlist[index];
            break;
        }
    }
    if (!target) {
        luaL_argerror(L, 1, "thread ID not found");
        return 0;
    }
    if (target->state == L) {
        luaL_argerror(L, 1, "attempted to kill self");
        return 0;
    }
    lua_resetthread(target->state);
    lua_pushthread(target->state);
    luaL_unref(target->state, 1, target->ref);
    lua_pop(target->state, 1);
    free(target);
    arrdel(threadlist, index);
    return 0;
}

static const struct luaL_Reg lua_funcs [] = {
    {"_PTGetMillis", lua_pt_get_millis},
    {"_PTPlayBeep", lua_pt_play_beep},
    {"_PTStopBeep", lua_pt_stop_beep},
    {"_PTSleep", lua_pt_sleep},
    {"_PTStartThread", lua_pt_start_thread},
    {"_PTStopThread", lua_pt_stop_thread},
    {NULL, NULL},
};

int script_exec() {
    // Iterate through stack of threads
    for (int i = 0; i < arrlen(threadlist); i++) {
        struct script_thread *target = threadlist[i];
        int nres = 0;
        if (target->sleep_until > timer_millis())
            continue;
        printf("resuming %d\n", target->thread_id);
        int result = lua_resume(target->state, NULL, 0, &nres);
        if (result == LUA_OK) {
            // thread finished, clean up and remove
            printf("done, freeing %d\n", target->thread_id);
            lua_closethread(target->state, NULL);
            free(target);
            arrdel(threadlist, i);
            i--;
            continue;
        } else if (result == LUA_YIELD) {
            // thread yielded, put it back
            printf("yielding %d\n", target->thread_id);
            continue;
        } else {
            // lua error
            printf("script_exec(): - %s\n", lua_tostring(target->state, -1));
            luaL_traceback(main_thread, target->state, NULL, 1);
            printf("%s", lua_tostring(main_thread, 1));
            lua_pop(main_thread, 1);
            exit(1);
        }
    }
    return arrlen(threadlist);
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

