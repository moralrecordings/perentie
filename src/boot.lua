
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

local _PTThreads = {}
local _PTThreadMaxID = 0
PTStartThread = function (name, func) 
    if _PTThreads[name] then
        error(string.format("PTStartThread(): thread named %s exists", name));
    end
    _PTThreadMaxID = _PTThreadMaxID + 1;
    _PTThreads[name] = _PTThreadMaxID;
    _PTStartThread(_PTThreads[name], func);
end

PTStopThread = function (name) 
    if not _PTThreads[name] then
        error(string.format("PTStopThread(): thread named %s doesn't exist", name));
    end
    _PTStopThread(_PTThreads[name]);
    _PTThreads[name] = nil;
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
