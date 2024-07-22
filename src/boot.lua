
-- Initial engine setup

PTGetMillis = function ()
    return _PTGetMillis();
end

PTPlayBeep = function (freq)
    return _PTPlayBeep(freq);
end

PTStopBeep = function ()
    return _PTStopBeep();
end

PTSleep = function (delay)
    _PTSleep(delay);
    coroutine.yield();
end

local _PTOnlyRunOnce = {};
PTOnlyRunOnce = function (name)
    if _PTOnlyRunOnce[name] then
        error(string.format("PTOnlyRunOnce(): attempted to run %s twice!", name));
    end
    _PTOnlyRunOnce[name] = 1;
end

local _PTThreads = {};
local _PTThreadsSleepUntil = {}
PTStartThread = function (name, func) 
    if _PTThreads[name] then
        error(string.format("PTStartThread(): thread named %s exists", name));
    end
    _PTThreads[name] = coroutine.create(func);
end

PTStopThread = function (name) 
    if not _PTThreads[name] then
        error(string.format("PTStopThread(): thread named %s doesn't exist", name));
    end
    coroutine.close(_PTThreads[name]);
    _PTThreads[name] = nil;
end

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

_PTRunThreads = function ()
    count = 0;
    for name, thread in pairs(_PTThreads) do
        is_awake = true;
        if _PTThreadsSleepUntil[name] then
            if _PTThreadsSleepUntil[name] > _PTGetMillis() then
                is_awake = false
            else
                _PTThreadsSleepUntil[name] = nil;
            end
        end
        if is_awake then
            success, result = coroutine.resume(thread);
            if not success then
                print(string.format("PTRunThreads(): Thread %s errored: %s", name, result));
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

local _PTCurrentRoom = nil;
local _PTOnRoomEnterHandlers = {};
local _PTOnRoomExitHandlers = {};
PTOnRoomEnter = function (name, func)
    if _PTOnRoomEnterHandlers[name] then
        print(string.format("PTOnRoomEnter: overwriting handler for %s"));
    end
    _PTOnRoomEnterHandlers[name] = func;
end

PTOnRoomExit = function (name, func)
    if _PTOnRoomExitHandlers[name] then
        print(string.format("PTOnRoomExit: overwriting handler for %s"));
    end
    _PTOnRoomExitHandlers[name] = func;
end


PTSwitchRoom = function (name, ctx)
    if (_PTCurrentRoom and _PTOnRoomExitHandlers[_PTCurrentRoom]) then
        _PTOnRoomExitHandlers[_PTCurrentRoom](ctx);
    end
    _PTCurrentRoom = name;
    if (_PTCurrentRoom and _PTOnRoomEnterHandlers[_PTCurrentRoom]) then
        _PTOnRoomEnterHandlers[_PTCurrentRoom](ctx);
    end
end
