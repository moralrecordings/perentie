# Debugging

## Logging

Perentie provides a simple API for log messages:

```lua
local errcode = 10
local errormsg = "a mistake"
PTLog("something went wrong: %s, value=%d\n", errormsg, errcode)
```

`PTLog` uses the same syntax as [Lua's string.format](https://www.luadocs.com/docs/functions/string/format). If you need to print the contents of a table, you can run them through the bundled copy of [inspect.lua](https://github.com/kikito/inspect.lua), which does a good job converting these to human-readable strings. Be aware that this conversion can be expensive in terms of cycles.

You can enable logging by passing the `--log` command line option. For DOS builds, `PTLog` will write logs to the file `perentie.log`. For SDL builds, `PTLog` will print logs to the console.


## Lua shell

Perentie includes a built-in Lua shell, accessible over a COM port via a null-modem connection. For DOSBox Staging users, all that's required is to add the following line to your dosbox.conf: 

```
[serial]
serial4       = nullmodem telnet:1 port:42424
```

In your game's Lua code, add the following call:

```lua
PTSetDebugConsole(true, "COM4")
```

We **do not** recommend enabling this in your release builds. Real DOS machines tend to use COM ports for communicating with hardware, and reading an endless barrage of nonsense from the COM lines will tank performance.

When the engine is running, you can connect to the shell on port 42424 using a Telnet client:

```
$ telnet localhost 42424
Trying 127.0.0.1...
Connected to localhost.
Escape character is '^]'.

┈┅━┥ Perentie v0.9.0 - Console ┝━┅┈
Lua 5.4.7  Copyright (C) 1994-2024 Lua.org, PUC-Rio

>> PTVersion()
"0.9.0"
>> 
```

This provides a very basic REPL interface from inside the main thread of the engine, similar to the one you get from running the `lua` CLI. Results from any commands you run will be printed to the shell; these are filtered through [inspect.lua](https://github.com/kikito/inspect.lua) so that e.g. tables will display by default. In order to print to the debug shell from your game code, you can use the `print` function.
