
// Hacked up version of the Lua stand-alone interpreter
// to run over Telnet.

#include "lua/lprefix.h"

#include <fcntl.h>
#include <signal.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include "lua/lua.h"

#include "lua/lauxlib.h"
#include "lua/lualib.h"

#include "log.h"
#include "system.h"
#include "version.h"

#define PROGNAME "perentie"

static char line_buffer[1024] = { 0 };
static int line_end = 0;
static bool repl_activated = false;
static bool repl_in_multiline = false;

static int lua_serial_print(lua_State* L);

void repl_init(lua_State* L)
{
    // Intercept the "print" function to dump to the console
    lua_pushcfunction(L, lua_serial_print);
    lua_setglobal(L, "print");
}

/*
** Prints an error message, adding the program name in front of it
** (if present)
*/
static void l_message(const char* pname, const char* msg)
{
    if (pname)
        pt_sys.serial->printf("%s: ", pname);
    pt_sys.serial->printf("%s\n", msg);
}

/*
** Check whether 'status' is not OK and, if so, prints the error
** message on the top of the stack.
*/
static int report(lua_State* L, int status)
{
    if (status != LUA_OK) {
        const char* msg = lua_tostring(L, -1);
        if (msg == NULL)
            msg = "(error message not a string)";
        l_message(PROGNAME, msg);
        lua_pop(L, 1); /* remove message */
    }
    return status;
}

static int lua_serial_print(lua_State* L)
{
    int n = lua_gettop(L); /* number of arguments */
    int i;
    for (i = 1; i <= n; i++) { /* for each argument */
        size_t l;
        lua_getglobal(L, "inspect");
        lua_pushvalue(L, i);
        lua_call(L, 1, 1);
        const char* s = lua_tolstring(L, -1, &l); /* convert it to string */
        if (i > 1) /* not the first element? */
            pt_sys.serial->write("\t", 1); /* add a tab before it */
        pt_sys.serial->write(s, l); /* print it */
        lua_pop(L, 1); /* pop result */
    }
    pt_sys.serial->write("\n", 1);
    return 0;
}

/*
** Prints (calling the Lua 'print' function) any values on the stack
*/
static void l_print(lua_State* L)
{
    int n = lua_gettop(L);
    if (n > 0) { /* any result to be printed? */
        luaL_checkstack(L, LUA_MINSTACK, "too many results to print");
        lua_pushcfunction(L, lua_serial_print);
        lua_insert(L, 1);
        if (lua_pcall(L, n, 0, 0) != LUA_OK)
            l_message(PROGNAME, lua_pushfstring(L, "error printing the output (%s)", lua_tostring(L, -1)));
    }
}

// Push the current line onto the Lua stack.
static void repl_pushline(lua_State* L, char* line, size_t l)
{
    if (l > 0 && line[l - 1] == '\n') // line ends with newline?
        line[--l] = '\0'; // remove it
    lua_pushlstring(L, line, l);
}

// Try to compile line on the stack as 'return <line>;'; on return, stack
// has original line and the compiled chunk/error message.
static int repl_addreturn(lua_State* L)
{
    const char* line = lua_tostring(L, -1); /* original line */
    const char* retline = lua_pushfstring(L, "return %s;", line);
    int status = luaL_loadbuffer(L, retline, strlen(retline), "=stdin");
    lua_remove(L, -2); // remove modified line

    return status;
}

/*
** Check whether 'status' signals a syntax error and the error
** message at the top of the stack ends with the above mark for
** incomplete statements.
*/
static int repl_incomplete(lua_State* L, int status)
{
    if (status == LUA_ERRSYNTAX) {
        size_t lmsg;
        const char* msg = lua_tolstring(L, -1, &lmsg);
        log_print("finding the mistake: %s\n", msg);
        if (strstr(msg, "<eof> expected") || strstr(msg, "near <eof>")) {
            return 1;
        }
    }

    return 0; /* else... */
}

/*
** Message handler used to run all chunks
*/
static int msghandler(lua_State* L)
{
    const char* msg = lua_tostring(L, 1);
    if (msg == NULL) { /* is error object not a string? */
        if (luaL_callmeta(L, 1, "__tostring") && /* does it have a metamethod */
            lua_type(L, -1) == LUA_TSTRING) /* that produces a string? */
            return 1; /* that is the message */
        else
            msg = lua_pushfstring(L, "(error object is a %s value)", luaL_typename(L, 1));
    }
    luaL_traceback(L, L, msg, 1); /* append a standard traceback */
    return 1; /* return the traceback */
}

/*
 ** Interface to 'lua_pcall', which sets appropriate message function
 ** and C-signal handler. Used to run all chunks.
 */
static int docall(lua_State* L, int narg, int nres)
{
    int status;
    int base = lua_gettop(L) - narg; /* function index */
    lua_pushcfunction(L, msghandler); /* push message handler */
    lua_insert(L, base); /* put it under function and args */
    // globalL = L;  /* to be available to 'laction' */
    // setsignal(SIGINT, laction);  /* set C-signal handler */
    status = lua_pcall(L, narg, nres, base);
    // setsignal(SIGINT, SIG_DFL); /* reset C-signal handler */
    lua_remove(L, base); /* remove message handler from the stack */
    return status;
}

void repl_process_line(lua_State* L, char* line, size_t size)
{
    int status = -1;

    // User has hit enter.
    if (!repl_in_multiline) {
        // We're in single line edit mode.
        // Push line to Lua stack
        repl_pushline(L, line, size);

        status = repl_addreturn(L);
        if (lua_gettop(L) != 2) {
            log_print("repl_process_line: expected 2 elements, got %d", lua_gettop(L));
            exit(1);
        }
        if (repl_incomplete(L, status)) {
            // log_print("incomplete, stashing\n");
            repl_in_multiline = true;
            // Ditch the error message
            lua_remove(L, -1);
            // Stash the line in a global
            lua_setglobal(L, "_repl_buffer");
            return;
        }
        // Remove the original line
        lua_remove(L, -2);
    } else {
        // We're in multi-line edit mode.
        // Get the multi-line input from the buffer
        lua_getglobal(L, "_repl_buffer");
        // Add a newline
        lua_pushliteral(L, "\n");
        // Get the newest line
        repl_pushline(L, line, size);
        // Join them up
        lua_concat(L, 3);
        if (lua_gettop(L) != 1) {
            log_print("repl_process_line: expected 1 element, got %d", lua_gettop(L));
            exit(1);
        }

        size_t len;
        const char* chunk = lua_tolstring(L, 1, &len);
        // log_print("trying chunk: %s\n", chunk);
        //  Try compiling it to a chunk
        status = luaL_loadbuffer(L, chunk, len, "=repl");
        // Message incomplete, still needs more
        if (repl_incomplete(L, status)) {
            // log_print("incomplete, stashing\n");
            //  Ditch the error message
            lua_remove(L, -1);
            // Stash the line in a global
            lua_setglobal(L, "_repl_buffer");
            // add to history
            return;
        } else {
            // Either a success or an error, leave multiline
            repl_in_multiline = false;
            // Remove the original line
            lua_remove(L, -2);
        }
    }

    // log_print("check result: %d\n", status);
    //  If the status is okay, we must have a single stack
    //  item containing the block of code to run.
    if (status == LUA_OK) {
        status = docall(L, 0, LUA_MULTRET);
        // log_print("do result: %d\n", status);
    }
    if (status == LUA_OK) {
        l_print(L); // print and remove anything that's on the stack
    } else {
        report(L, status); // print and remove the error message
    }
    if (lua_gettop(L) != 0) {
        log_print("repl_process_line: expected 0 elements, got %d", lua_gettop(L));
        exit(1);
    }
}

void repl_update(lua_State* L)
{
    char* line;

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
    //      - save it to the buffer
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

    // Check the serial port to see if there's any input.
    // We're using telnet, so this should be buffered client-side
    // until they hit Enter.
    if (pt_sys.serial->rx_ready()) {
        line_end = pt_sys.serial->gets(line_buffer, sizeof(line_buffer) - 1);
    }

    if (line_end > 0) {
        line_buffer[line_end] = '\0';

        // for (int i = 0; i < line_end; i++) {
        //     log_print("%02X ", line_buffer[i]);
        // }
        // log_print("\n");
        //  We've opened up telnet and typed something for the first time;
        //  show a welcome message and the prompt.
        if (!repl_activated) {
            repl_activated = true;
            // Show prompt
            pt_sys.serial->printf("┈┅━┥ Perentie v%s - Console ┝━┅┈\n", VERSION);
            pt_sys.serial->printf("%s\n\n", LUA_COPYRIGHT);
            pt_sys.serial->printf("%s", repl_in_multiline ? " ... > " : ">> ");
            log_print("repl_update: console activated!\n");
            line_end = 0;
            return;
        }

        log_print("line_buffer: %s\n", line_buffer);

        int idx_start = 0;
        int idx = 0;
        while (idx < line_end) {
            if (line_buffer[idx] == '\0')
                break;
            if (line_buffer[idx] == '\n') {
                repl_process_line(L, line_buffer + idx_start, idx - idx_start + 1);
                idx_start = idx + 1;
            }

            idx++;
        }
        if (idx - idx_start)
            repl_process_line(L, line_buffer + idx_start, idx - idx_start + 1);

        pt_sys.serial->printf("%s", repl_in_multiline ? " ... > " : ">> ");
    }
    // reset the buffer
    line_end = 0;
}

void repl_shutdown()
{
}
