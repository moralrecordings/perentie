
-- Initial engine setup

--- Get the version number of the Perentie engine.
-- @treturn string Version string.
PTVersion = function ()
    return _PTVersion();
end

--- Get the number of milliseconds elapsed since the engine started.
-- @treturn int Number of milliseconds.
PTGetMillis = function ()
    return _PTGetMillis();
end

--- Play a tone on the PC speaker.
-- The note will play until it is stopped by a call to PTStopBeep.
-- @tparam number freq Audio frequency of the tone.
PTPlayBeep = function (freq)
    return _PTPlayBeep(freq);
end

--- Stop playing a tone on the PC speaker.
PTStopBeep = function ()
    return _PTStopBeep();
end

local _PTOnlyRunOnce = {};
--- Assert that this function can't be run more than once.
-- Usually used as a guard instruction at the top of a Lua script.
-- Multiple invocations will raise an error.
-- @tparam string name Name of the script.
PTOnlyRunOnce = function (name)
    if _PTOnlyRunOnce[name] then
        error(string.format("PTOnlyRunOnce(): attempted to run %s twice!", name));
    end
    _PTOnlyRunOnce[name] = 1;
end

--- Start a function in a new thread.
-- Perentie runs threads with cooperative multitasking; that is,
-- a long-running thread must use a sleep function like PTSleep
-- to yield control back to the engine.
-- Perentie will exit when all threads have stopped.
-- @tparam name string Name of the thread.
-- @tparam func function Function body to run.
local _PTThreads = {};
local _PTThreadsSleepUntil = {}
PTStartThread = function (name, func) 
    if _PTThreads[name] then
        error(string.format("PTStartThread(): thread named %s exists", name));
    end
    _PTThreads[name] = coroutine.create(func);
end

--- Stop a running thread.
-- @tparam name string Name of the thread.
PTStopThread = function (name) 
    if not _PTThreads[name] then
        error(string.format("PTStopThread(): thread named %s doesn't exist", name));
    end
    coroutine.close(_PTThreads[name]);
    _PTThreads[name] = nil;
end

--- Sleep the current thread.
-- Lua uses co-operative multitasking; for long-running background 
-- tasks it is important to call a sleep function whenever possible,
-- even with a delay of 0.
-- Not doing so will cause the scripting engine to freeze up.
-- @tparam int delay Time to wait in milliseconds.
PTSleep = function (millis)
    thread, _ = coroutine.running();
    for k, v in pairs(_PTThreads) do
        if v == thread then
            _PTThreadsSleepUntil[k] = PTGetMillis() + millis;
            coroutine.yield();
            return;
        end
    end
    error(string.format("PTSleep(): thread not found"));
end

local _PTCurrentRoom = nil;
local _PTOnRoomEnterHandlers = {};
local _PTOnRoomExitHandlers = {};
--- Set a callback for switching to a room.
-- @tparam name string Name of the room.
-- @tparam func function Function body to call, with an optional argument
-- for context data.
PTOnRoomEnter = function (name, func)
    if _PTOnRoomEnterHandlers[name] then
        print(string.format("PTOnRoomEnter: overwriting handler for %s"));
    end
    _PTOnRoomEnterHandlers[name] = func;
end

--- Set a callback for switching away from a room.
-- @tparam name string Name of the room.
-- @tparam func function Function body to call, with an optional argument
-- for context data.
PTOnRoomExit = function (name, func)
    if _PTOnRoomExitHandlers[name] then
        print(string.format("PTOnRoomExit: overwriting handler for %s"));
    end
    _PTOnRoomExitHandlers[name] = func;
end

--- Return the name of the current room
-- @treturn string Name of the room.
PTCurrentRoom = function ()
    return _PTCurrentRoom;
end

--- Switch the current room.
-- Will call the callbacks specified by PTOnRoomEnter and
-- PTOnRoomExit.
-- @tparam name string Name of the room.
-- @tparam ctx any Optional Context data to pass to the callbacks.
PTSwitchRoom = function (name, ctx)
    if (_PTCurrentRoom and _PTOnRoomExitHandlers[_PTCurrentRoom]) then
        _PTOnRoomExitHandlers[_PTCurrentRoom](ctx);
    end
    _PTCurrentRoom = name;
    if (_PTCurrentRoom and _PTOnRoomEnterHandlers[_PTCurrentRoom]) then
        _PTOnRoomEnterHandlers[_PTCurrentRoom](ctx);
    end
end

local _PTToggleWatchdog = true;
--- Toggle the use of the watchdog to abort threads that take too long.
-- Enabled by default.
-- @tparam bool enable Whether to enable the watchdog.
PTToggleWatchdog = function (enable)
    _PTToggleWatchdog = enable;
end

-- Stuff called by the C engine main loop. 

--- Hook callback for when a thread runs too many instructions
-- without sleeping. Throws an error.
_PTWatchdog = function (event)
    if event == "count" then
        info = debug.getinfo(2, "Sl");
        error(string.format("PTWatchdog(): woof! woooooff!!! %s:%d took too long", info.source, info.currentline));
    end
end
--- Run and handle execution for all of the threads.
-- @treturn int Number of threads still alive.
_PTRunThreads = function ()
    count = 0;
    for name, thread in pairs(_PTThreads) do
        -- Check if the thread is supposed to be asleep
        is_awake = true;
        if _PTThreadsSleepUntil[name] then
            if _PTThreadsSleepUntil[name] > _PTGetMillis() then
                is_awake = false
            else
                _PTThreadsSleepUntil[name] = nil;
            end
        end
        if is_awake then
            -- Tell the watchdog to chomp the thread if we take longer than 10000 instructions.
            if _PTToggleWatchdog then
                debug.sethook(thread, _PTWatchdog, "", 10000);
            end
            -- Resume the thread, let it run until the next sleep command
            success, result = coroutine.resume(thread);
            -- Pet the watchdog for a job well done
            if _PTToggleWatchdog then
                debug.sethook(thread);
            end

            -- Handle the response from the execution run.
            if not success then
                print(string.format("PTRunThreads(): Thread %s errored:\n  %s", name, result));
                debug.traceback(_PTThreads[name]);
            end
            status = coroutine.status(thread);
            if status == "dead" then
                print(string.format("PTRunThreads(): Thread %s terminated", name));
                coroutine.close(_PTThreads[name]);
                _PTThreads[name] = nil;
            else
                count = count + 1;
            end
        else
            count = count + 1;
        end
    end
    return count;
end
