
// Hacked up version of the Lua stand-alone interpreter
// to run over Telnet.

#include "lua/lprefix.h"


#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <signal.h>
#include <unistd.h>
#include <time.h>

#include "lua/lua.h"

#include "lua/lauxlib.h"
#include "lua/lualib.h"

#include "linenois/linenois.h"

#include "log.h"
#include "version.h"

#define lua_saveline(L,line)	((void)L, linenoiseHistoryAdd(line))

#define PROGNAME "perentie"

struct linenoiseState ln_state = {0};
char line_buffer[1024] = {0}; 
bool repl_activated = false;
bool repl_in_multiline = false;
FILE *repl_fp = NULL;

void repl_init() {
    int repl_fd = open("COM4", O_RDWR);
    repl_fp = fdopen(repl_fd, "w");
    linenoiseEditStart(&ln_state, repl_fd, repl_fd, line_buffer, sizeof(line_buffer), ""); 

    linenoiseHistorySetMaxLen(100);
}


/*
** Prints an error message, adding the program name in front of it
** (if present)
*/
static void l_message (const char *pname, const char *msg) {
  if (pname) lua_writestringerror("%s: ", pname);
  lua_writestringerror("%s\n", msg);
}


/*
** Check whether 'status' is not OK and, if so, prints the error
** message on the top of the stack.
*/
static int report (lua_State *L, int status) {
  if (status != LUA_OK) {
    const char *msg = lua_tostring(L, -1);
    if (msg == NULL)
      msg = "(error message not a string)";
    l_message(PROGNAME, msg);
    lua_pop(L, 1);  /* remove message */
  }
  return status;
}


/*
** Prints (calling the Lua 'print' function) any values on the stack
*/
static void l_print (lua_State *L) {
  int n = lua_gettop(L);
  if (n > 0) {  /* any result to be printed? */
    luaL_checkstack(L, LUA_MINSTACK, "too many results to print");
    lua_getglobal(L, "print");
    lua_insert(L, 1);
    if (lua_pcall(L, n, 0, 0) != LUA_OK)
      l_message(PROGNAME, lua_pushfstring(L, "error calling 'print' (%s)",
                                             lua_tostring(L, -1)));
  }
}




// Push the current line onto the Lua stack.
static void repl_pushline (lua_State *L, char *line) {
  size_t l = strlen(line);
  if (l > 0 && line[l-1] == '\n')  // line ends with newline?
    line[--l] = '\0';  // remove it
  lua_pushlstring(L, line, l);
  free(line);
}

// Try to compile line on the stack as 'return <line>;'; on return, stack
// has either compiled chunk or original line (if compilation failed).
static int repl_addreturn (lua_State *L) {
  const char *line = lua_tostring(L, -1);  /* original line */
  const char *retline = lua_pushfstring(L, "return %s;", line);
  int status = luaL_loadbuffer(L, retline, strlen(retline), "=stdin");
  if (status == LUA_OK) {
    lua_remove(L, -2);  /* remove modified line */
    if (line[0] != '\0')  /* non empty? */
      linenoiseHistoryAdd(line);  /* keep history */
  }
  else
    lua_pop(L, 2);  /* pop result from 'luaL_loadbuffer' and modified line */
  return status;
}

/* mark in error messages for incomplete statements */
#define EOFMARK		"<eof>"
#define marklen		(sizeof(EOFMARK)/sizeof(char) - 1)


/*
** Check whether 'status' signals a syntax error and the error
** message at the top of the stack ends with the above mark for
** incomplete statements.
*/
static int repl_incomplete (lua_State *L, int status) {
  if (status == LUA_ERRSYNTAX) {
    size_t lmsg;
    const char *msg = lua_tolstring(L, -1, &lmsg);
    if (lmsg >= marklen && strcmp(msg + lmsg - marklen, EOFMARK) == 0) {
      lua_pop(L, 1);
      return 1;
    }
  }

  return 0;  /* else... */
}

/*
** Message handler used to run all chunks
*/
static int msghandler (lua_State *L) {
  const char *msg = lua_tostring(L, 1);
  if (msg == NULL) {  /* is error object not a string? */
    if (luaL_callmeta(L, 1, "__tostring") &&  /* does it have a metamethod */
        lua_type(L, -1) == LUA_TSTRING)  /* that produces a string? */
      return 1;  /* that is the message */
    else
      msg = lua_pushfstring(L, "(error object is a %s value)",
                               luaL_typename(L, 1));
  }
  luaL_traceback(L, L, msg, 1);  /* append a standard traceback */
  return 1;  /* return the traceback */
}

  /*
** Interface to 'lua_pcall', which sets appropriate message function
** and C-signal handler. Used to run all chunks.
*/
static int docall (lua_State *L, int narg, int nres) {
  int status;
  int base = lua_gettop(L) - narg;  /* function index */
  lua_pushcfunction(L, msghandler);  /* push message handler */
  lua_insert(L, base);  /* put it under function and args */
  //globalL = L;  /* to be available to 'laction' */
  //setsignal(SIGINT, laction);  /* set C-signal handler */
  status = lua_pcall(L, narg, nres, base);
  //setsignal(SIGINT, SIG_DFL); /* reset C-signal handler */
  lua_remove(L, base);  /* remove message handler from the stack */
  return status;
}


void repl_update(lua_State *L) {
    char *line;
    fd_set readfds;
    FD_ZERO(&readfds);
    FD_SET(ln_state.ifd, &readfds);
    struct timeval tv;
    tv.tv_sec = 0;
    tv.tv_usec = 0;

    // This whole thing needs to be asynchronous.
    // - see loadline() 
    // - read the line from telnet, character by character
    //    - see pushline()
    // - remove the newline at the end
    // ------------------- cut point
    // - push the buffer to the lua stack
    // - wrap it with "return %s;" (addreturn())
    // - attempt to run it with luaL_loadbuffer
    //    - if that doesn't work:
    //      - remove it from the lua stack
    //      - save it to the linenoise buffer
    //      - try again but as a multiline
    //      -------------------------- cut point
    //    - else
    //      - remove everything from the stack except the last function
    //      - run it with lua_pcall
    //      - print any results with l_print()
    //      - print any errors with report()

    // Main thing that needs to change is the bit involving keeping
    // stuff on the Lua stack between updates. In multiline mode we
    // need to stash the transient state into a global, so the
    // stack is clean for doing other things on the main thread.

    // Poll the file descriptor to see if there are any incoming bytes.
    int result = select(ln_state.ifd+1,  &readfds, NULL, NULL, &tv);
    if (result == -1) {
        log_print("select failed\n");
        return;
    } else if (result) {
        // We've opened up telnet and typed a single character;
        // show a welcome message and the prompt.
        if (!repl_activated) {
            repl_activated = true;
            linenoiseHide(&ln_state);
            fprintf(repl_fp, "┈┅━┥ Perentie v%s - Console ┝━┅┈\n", VERSION);
            fprintf(repl_fp, "%s\n\n", LUA_COPYRIGHT);
            ln_state.prompt = PROGNAME " > ";
            linenoiseShow(&ln_state);
        }

        // There's some data in the file descriptor, let linenoise process it
        line = linenoiseEditFeed(&ln_state);
        if (line == linenoiseEditMore) {
            // user hasn't hit enter yet, we need more!
            return;
        } else if (line == NULL) {
            // user has entered Ctrl-C or Ctrl-D
            return;
        }

        int status = -1;

        // User has hit enter.
        if (!repl_in_multiline) {
            // We're in single line edit mode.
            // Push line to Lua stack
            repl_pushline(L, line);

            status = repl_addreturn(L);
            lua_remove(L, 1);
            lua_assert(lua_gettop(L) == 1);
            if (status != LUA_OK) {
                repl_in_multiline = true;
                // Stash the line in a global
                lua_setglobal(L, "_repl_buffer");
            }
        } else {
            // We're in multi-line edit mode. 
            repl_pushline(L, line);
            // Get the multi-line input from the buffer 
            lua_getglobal(L, "_repl_buffer");
            size_t len;
            const char *chunk = lua_tolstring(L, 1, &len);
            // Try compiling it to a chunk
            status = luaL_loadbuffer(L, chunk, len, "=repl");
            if (!repl_incomplete(L, status)) {
              lua_setglobal(L, "_repl_buffer");
              linenoiseHistoryAdd(line);  /* keep history */
            } else {
              // Statement complete; join it up
              lua_pushliteral(L, "\n");  /* add newline... */
              lua_insert(L, -2);  /* ...between the two lines */
              lua_concat(L, 3);  /* join them */
            }
        }

        // If the status is okay, we must have a single stack
        // item containing the block of code to run.
        if (status == LUA_OK) {
          status = docall(L, 0, LUA_MULTRET);
          if (status == LUA_OK) {
            l_print(L);
          } else {
            report(L, status);
          }
        }
    }
}

void repl_shutdown() {
    linenoiseEditStop(&ln_state);
}

