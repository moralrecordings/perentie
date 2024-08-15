#ifndef PERENTIE_REPL_H
#define PERENTIE_REPL_H

typedef struct lua_State lua_State;

void repl_init(lua_State* L);
void repl_update(lua_State* L);
void repl_shutdown();

#endif
