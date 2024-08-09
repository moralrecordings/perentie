--- Initial engine setup

SCREEN_WIDTH = 320
SCREEN_HEIGHT = 200

--- Get the version number of the Perentie engine.
-- @treturn string Version string.
PTVersion = function()
    return _PTVersion()
end

--- Print a message to the logging device. Use this instead of
-- Lua's print().
-- @tparam string formatstring Format string in sprintf syntax.
-- @tparam any arg .
PTLog = function(...)
    return _PTLog(string.format(...))
end

--- Object constructors

--- Create a new image.
-- @tparam string path Path of the image (must be indexed PNG).
-- @tparam int origin_x Origin x coordinate, relative to top-left corner. Default is 0.
-- @tparam int origin_y Origin y coordinate, relative to top-left corner. Default is 0.
-- @tparam int colourkey Palette index to use as colourkey. Default is -1.
-- @tresult table The new image.
PTImage = function(path, origin_x, origin_y, colourkey)
    if not origin_x then
        origin_x = 0
    end
    if not origin_y then
        origin_y = 0
    end
    if not colourkey then
        colourkey = -1
    end
    return { _type = "PTImage", ptr = _PTImage(path, origin_x, origin_y, colourkey) }
end

PTGetImageDims = function(image)
    if not image or image._type ~= "PTImage" then
        return 0, 0
    end
    return _PTGetImageDims(image.ptr)
end

PTGetImageOrigin = function(image)
    if not image or image._type ~= "PTImage" then
        return 0, 0
    end
    return _PTGetImageOrigin(image.ptr)
end

PTSetImageOrigin = function(image, x, y)
    if not image or image._type ~= "PTImage" then
        return
    end
    return _PTSetImageOrigin(image.ptr, x, y)
end

--- Create a new bitmap font.
-- @tparam string path Path of the bitmap font (must be BMFont V3 binary).
-- @tresult table The new font.
PTFont = function(path)
    return { _type = "PTFont", ptr = _PTFont(path) }
end

--- Create an new image containing rendered text.
-- @tparam string text Unicode text to render.
-- @tparam table font Font object to use.
-- @tparam int width Width of bounding area in pixels. Default is 200.
-- @tparam str align Text alignment; one of "left", "center" or "right". Defaults to "left".
-- @tparam table colour Inner colour; list of 3 8-bit numbers. Default is white.
-- @tresult table The new image.
PTText = function(text, font, width, align, colour)
    if not width then
        width = 200
    end
    local align_enum = 1
    if align == "center" then
        align_enum = 2
    elseif align == "right" then
        align_enum = 3
    end
    local r = 0xff
    local g = 0xff
    local b = 0xff
    if colour and colour[1] then
        r = colour[1]
    end
    if colour and colour[2] then
        g = colour[2]
    end
    if colour and colour[3] then
        b = colour[3]
    end

    return { _type = "PTImage", ptr = _PTText(text, font.ptr, width, align_enum, r, g, b) }
end

_PTGlobalRenderList = {}
PTGlobalAddObject = function(object)
    if object then
        table.insert(_PTGlobalRenderList, object)
    end
    table.sort(_PTGlobalRenderList, function(a, b)
        return a.z < b.z
    end)
end

--- Create a new room.
-- @tparam string name Name of the room.
-- @tparam int width Width of the room in pixels.
-- @tparam int height Height of the room in pixels.
-- @tresult table The new room.
PTRoom = function(name, width, height)
    return { _type = "PTRoom", name = name, width = width, height = height, render_list = {} }
end

PTRoomAddObject = function(room, object)
    if not room or room._type ~= "PTRoom" then
        error("PTRoomAddObject: expected PTRoom for first argument")
        return
    end
    if object then
        table.insert(room.render_list, object)
    end
    table.sort(room.render_list, function(a, b)
        return a.z < b.z
    end)
end

PTRoomRemoveObject = function(room, object)
    if not room or room._type ~= "PTRoom" then
        error("PTRoomAddObject: expected PTRoom for first argument")
    end
    if object then
        for i, obj in pairs(room.render_list) do
            if object == obj then
                table.remove(room.render_list, i)
                break
            end
        end
    end
    table.sort(room.render_list, function(a, b)
        return a.z < b.z
    end)
end

PTActor = function(name)
    return {
        _type = "PTActor",
        name = name,
        x = 0,
        y = 0,
        z = 0,
        talk_x = 0,
        talk_y = 0,
        talk_img = nil,
        talk_font = nil,
        talk_color = { 0xff, 0xff, 0xff },
        talk_millis = nil,
    }
end

PTActorUpdate = function(actor, fast_forward)
    if not actor or actor._type ~= "PTActor" then
        error("PTActor: expected PTActor for first argument")
    end
    if actor.talk_img and actor.talk_millis then
        if not fast_forward and _PTGetMillis() < actor.talk_millis then
            return false
        else
            PTRoomRemoveObject(PTCurrentRoom(), actor.talk_img)
            actor.talk_img = nil
        end
    end
    return true
end

TALK_BASE_DELAY = 1000
TALK_CHAR_DELAY = 85

PTActorTalk = function(actor, message)
    if not actor or actor._type ~= "PTActor" then
        error("PTActor: expected PTActor for first argument")
    end
    if not actor.talk_font then
        PTLog("PTActorTalk: no default font!!")
        return
    end
    local text = PTText(message, actor.talk_font, 200, actor.talk_color)

    local width, height = PTGetImageDims(text)
    PTSetImageOrigin(text, width / 2, height)
    local x = math.min(math.max(actor.x + actor.talk_x, width / 2), SCREEN_WIDTH - width / 2)
    local y = math.min(math.max(actor.y + actor.talk_y, height), SCREEN_HEIGHT)

    actor.talk_img = PTBackground(text, x, y, 10)
    actor.talk_millis = _PTGetMillis() + TALK_BASE_DELAY + #message * TALK_CHAR_DELAY
    -- TODO: Make room reference on actor?
    PTRoomAddObject(PTCurrentRoom(), actor.talk_img)
end

PTWaitForActor = function(actor)
    local thread, _ = coroutine.running()
    for k, v in pairs(_PTThreads) do
        if v == thread then
            _PTThreadsActorWait[k] = actor
            coroutine.yield()
            return
        end
    end
    error(string.format("PTWaitForActor(): thread not found"))
end

PTBackground = function(image, x, y, z)
    return { _type = "PTBackground", image = image, x = x, y = y, z = z, collision = false }
end

PTSprite = function(x, y, z, animations)
    return {
        _type = "PTSprite",
        x = x,
        y = y,
        z = z,
        animations = animations,
        current_animation = nil,
        collision = false,
    }
end

PTAnimation = function(rate, frames)
    return {
        _type = "PTAnimation",
        rate = rate,
        looping = false,
        bounce = false,
        frames = frames,
        current_frame = 0,
        next_wait = 0,
    }
end

PTGetMousePos = function()
    return _PTGetMousePos()
end

--- Get the number of milliseconds elapsed since the engine started.
-- @treturn int Number of milliseconds.
PTGetMillis = function()
    return _PTGetMillis()
end

--- Play a tone on the PC speaker.
-- The note will play until it is stopped by a call to PTStopBeep.
-- @tparam number freq Audio frequency of the tone.
PTPlayBeep = function(freq)
    return _PTPlayBeep(freq)
end

--- Stop playing a tone on the PC speaker.
PTStopBeep = function()
    return _PTStopBeep()
end

local _PTOnlyRunOnce = {}
--- Assert that this function can't be run more than once.
-- Usually used as a guard instruction at the top of a Lua script.
-- Multiple invocations will raise an error.
-- @tparam string name Name of the script.
PTOnlyRunOnce = function(name)
    if _PTOnlyRunOnce[name] then
        error(string.format("PTOnlyRunOnce(): attempted to run %s twice!", name))
    end
    _PTOnlyRunOnce[name] = 1
end

--- Start a function in a new thread.
-- Perentie runs threads with cooperative multitasking; that is,
-- a long-running thread must use a sleep function like PTSleep
-- to yield control back to the engine.
-- Perentie will exit when all threads have stopped.
-- @tparam name string Name of the thread.
-- @tparam func function Function body to run.
_PTThreads = {}
local _PTThreadsSleepUntil = {}
_PTThreadsActorWait = {}
_PTThreadsFastForward = {}
PTStartThread = function(name, func, ...)
    if _PTThreads[name] then
        error(string.format("PTStartThread(): thread named %s exists", name))
    end
    varargs = ...
    _PTThreads[name] = coroutine.create(function()
        func(table.unpack(varargs))
    end)
end

--- Stop a running thread.
-- @tparam name string Name of the thread.
PTStopThread = function(name)
    if not _PTThreads[name] then
        error(string.format("PTStopThread(): thread named %s doesn't exist", name))
    end
    coroutine.close(_PTThreads[name])
    _PTThreads[name] = nil
    _PTThreadsFastForward[name] = nil
end

--- Sleep the current thread.
-- Lua uses co-operative multitasking; for long-running background
-- tasks it is important to call a sleep function whenever possible,
-- even with a delay of 0.
-- Not doing so will cause the scripting engine to freeze up.
-- @tparam int delay Time to wait in milliseconds.
PTSleep = function(millis)
    local thread, _ = coroutine.running()
    for k, v in pairs(_PTThreads) do
        if v == thread then
            _PTThreadsSleepUntil[k] = PTGetMillis() + millis
            coroutine.yield()
            return
        end
    end
    error(string.format("PTSleep(): thread not found"))
end

local _PTCurrentRoom = nil
local _PTOnRoomEnterHandlers = {}
local _PTOnRoomExitHandlers = {}
--- Set a callback for switching to a room.
-- @tparam name string Name of the room.
-- @tparam func function Function body to call, with an optional argument
-- for context data.
PTOnRoomEnter = function(name, func)
    if _PTOnRoomEnterHandlers[name] then
        PTLog("PTOnRoomEnter: overwriting handler for %s", name)
    end
    _PTOnRoomEnterHandlers[name] = func
end

--- Set a callback for switching away from a room.
-- @tparam name string Name of the room.
-- @tparam func function Function body to call, with an optional argument
-- for context data.
PTOnRoomExit = function(name, func)
    if _PTOnRoomExitHandlers[name] then
        PTLog("PTOnRoomExit: overwriting handler for %s", name)
    end
    _PTOnRoomExitHandlers[name] = func
end

--- Return the current room
-- @treturn table Room data.
PTCurrentRoom = function()
    return _PTCurrentRoom
end

--- Switch the current room.
-- Will call the callbacks specified by PTOnRoomEnter and
-- PTOnRoomExit.
-- @tparam room table Room object.
-- @tparam ctx any Optional Context data to pass to the callbacks.
PTSwitchRoom = function(room, ctx)
    PTLog("PTSwitchRoom: %s, %s", tostring(room), tostring(_PTCurrentRoom))
    if _PTCurrentRoom and _PTOnRoomExitHandlers[_PTCurrentRoom.name] then
        _PTOnRoomExitHandlers[_PTCurrentRoom.name](ctx)
    end
    _PTCurrentRoom = room
    if _PTCurrentRoom and _PTOnRoomEnterHandlers[_PTCurrentRoom.name] then
        _PTOnRoomEnterHandlers[_PTCurrentRoom.name](ctx)
    end
    PTLog("PTSwitchRoom: %s, %s", tostring(room), tostring(_PTCurrentRoom))
end

local _PTVerbCallbacks = {}
PTOnVerb = function(verb_name, hotspot_id, callback)
    if not _PTVerbCallbacks[verb_name] then
        _PTVerbCallbacks[verb_name] = {}
    end
    _PTVerbCallbacks[verb_name][hotspot_id] = callback
end

local _PTCurrentVerb = nil
local _PTCurrentHotspot = nil
PTDoVerb = function(verb_name, hotspot_id)
    PTLog("PTDoVerb: %s %s\n", tostring(verb_name), tostring(hotspot_id))
    _PTCurrentVerb = verb_name
    _PTCurrentHotspot = hotspot_id
end

-- this could be a custom callback!!!
PTVerbReady = function()
    return (
        _PTCurrentVerb ~= nil
        and _PTCurrentHotspot ~= nil
        and _PTVerbCallbacks[_PTCurrentVerb]
        and _PTVerbCallbacks[_PTCurrentVerb][_PTCurrentHotspot]
    )
end

_PTRunVerb = function()
    if PTVerbReady() then
        if _PTThreads["__verb"] then
            -- interrupting another verb, stop the existing one
            PTLog("attempted to replace running verb thread with %s %s!", _PTCurrentVerb, _PTCurrentHotspot)
        else
            PTStartThread(
                "__verb",
                _PTVerbCallbacks[_PTCurrentVerb][_PTCurrentHotspot],
                _PTCurrentVerb,
                _PTCurrentHotspot
            )
        end
        _PTCurrentVerb = nil
        _PTCurrentHotspot = nil
    end
end

local _PTInputGrabbed = false
PTGrabInput = function()
    _PTInputGrabbed = true
end

PTReleaseInput = function()
    _PTInputGrabbed = false
end

local _PTToggleWatchdog = true
--- Toggle the use of the watchdog to abort threads that take too long.
-- Enabled by default.
-- @tparam bool enable Whether to enable the watchdog.
PTToggleWatchdog = function(enable)
    _PTToggleWatchdog = enable
end

local _PTWatchdogLimit = 10000
--- Set the number of Lua instructions that need to elapse without a sleep
-- before the watchdog targets a thread.
-- Defaults to 10000.
-- @tparam int count Number of instructions.
PTSetWatchdogLimit = function(count)
    _PTWatchdogLimit = count
end

--- Quit Perentie.
-- @param int retcode Return code. Defaults to 0.
PTQuit = function(retcode)
    if not retcode then
        retcode = 0
    end
    _PTQuit(retcode)
end

-- Stuff called by the C engine main loop.

--- Hook callback for when a thread runs too many instructions
-- without sleeping. Throws an error.
-- @tparam string event Event provided by the debug layer. Should always be "count".
_PTWatchdog = function(event)
    if event == "count" then
        local info = debug.getinfo(2, "Sl")
        error(string.format("PTWatchdog(): woof! woooooff!!! %s:%d took too long", info.source, info.currentline))
    end
end
--- Run and handle execution for all of the threads.
-- @treturn int Number of threads still alive.
_PTRunThreads = function()
    _PTRunVerb()
    local count = 0
    for name, thread in pairs(_PTThreads) do
        -- Check if the thread is supposed to be asleep
        local is_awake = true
        if _PTThreadsActorWait[name] then
            is_awake = PTActorUpdate(_PTThreadsActorWait[name], _PTThreadsFastForward[name])
            if is_awake then
                _PTThreadsActorWait[name] = nil
            end
        elseif _PTThreadsFastForward[name] then
            -- Ignore all sleeps, go at top speed.
        elseif _PTThreadsSleepUntil[name] then
            if _PTThreadsSleepUntil[name] > _PTGetMillis() then
                is_awake = false
            else
                _PTThreadsSleepUntil[name] = nil
            end
        end
        if is_awake then
            -- Tell the watchdog to chomp the thread if we take longer than 10000 instructions.
            if _PTToggleWatchdog then
                debug.sethook(thread, _PTWatchdog, "", _PTWatchdogLimit)
            end
            -- Resume the thread, let it run until the next sleep command
            local success, result = coroutine.resume(thread)
            -- Pet the watchdog for a job well done
            if _PTToggleWatchdog then
                debug.sethook(thread)
            end

            -- Handle the response from the execution run.
            if not success then
                PTLog("PTRunThreads(): Thread %s errored:\n  %s", name, result)
                debug.traceback(_PTThreads[name])
            end
            local status = coroutine.status(thread)
            if status == "dead" then
                PTLog("PTRunThreads(): Thread %s terminated", name)
                coroutine.close(_PTThreads[name])
                _PTThreads[name] = nil
                _PTThreadsFastForward[name] = nil
            else
                count = count + 1
            end
        else
            count = count + 1
        end
    end
    return count
end

local _PTMouseSprite = nil
PTSetMouseSprite = function(sprite)
    _PTMouseSprite = sprite
end

PTGetAnimationFrame = function(object)
    if object and object._type == "PTSprite" then
        anim = object.animations[object.current_animation]
        if anim then
            if anim.rate == 0 then
                -- Rate is 0, don't automatically change frames
                if anim.current_frame == 0 then
                    anim.current_frame = 1
                end
            elseif anim.current_frame == 0 then
                anim.current_frame = 1
                anim.next_wait = PTGetMillis() + (1000 / anim.rate)
            elseif PTGetMillis() > anim.next_wait then
                anim.current_frame = (anim.current_frame % #anim.frames) + 1
                anim.next_wait = PTGetMillis() + (1000 / anim.rate)
            end
            return anim.frames[anim.current_frame]
        end
    elseif object and object._type == "PTBackground" then
        return object.image
    end
    return nil
end

local _PTGlobalEventConsumers = {}
PTGlobalOnEvent = function(type, callback)
    _PTGlobalEventConsumers[type] = callback
end

local _PTAutoClearScreen = true
--- Set whether to clear the screen at the start of every frame
-- @tparam bool val true if the screen is to be cleared, false otherwise.
PTSetAutoClearScreen = function(val)
    _PTAutoClearScreen = val
end

local _PTMouseOver = nil
--- Get the current object which the mouse is hovering over
-- @treturn table The object (PTSprite, PTBackground), or nil.
PTGetMouseOver = function()
    return _PTMouseOver
end

local _PTMouseOverConsumer = nil
PTOnMouseOver = function(callback)
    _PTMouseOverConsumer = callback
end

_PTUpdateMouseOver = function()
    local mouse_x, mouse_y = PTGetMousePos()
    if not _PTCurrentRoom or _PTCurrentRoom._type ~= "PTRoom" then
        return
    end
    -- Need to iterate through objects in reverse draw order
    for i = #_PTGlobalRenderList, 1, -1 do
        local obj = _PTGlobalRenderList[i]
        if obj.collision then
            frame = PTGetAnimationFrame(obj)
            if frame and _PTImageTestCollision(frame.ptr, mouse_x - obj.x, mouse_y - obj.y) then
                if _PTMouseOver ~= obj then
                    _PTMouseOver = obj
                    if _PTMouseOverConsumer then
                        _PTMouseOverConsumer(_PTMouseOver)
                    end
                end
                return
            end
        end
    end
    for i = #_PTCurrentRoom.render_list, 1, -1 do
        local obj = _PTCurrentRoom.render_list[i]
        if obj.collision then
            frame = PTGetAnimationFrame(obj)
            if frame and _PTImageTestCollision(frame.ptr, mouse_x - obj.x, mouse_y - obj.y) then
                if _PTMouseOver ~= obj then
                    _PTMouseOver = obj
                    if _PTMouseOverConsumer then
                        _PTMouseOverConsumer(_PTMouseOver)
                    end
                end
                return
            end
        end
    end
    if _PTMouseOver ~= nil then
        _PTMouseOver = nil
        if _PTMouseOverConsumer then
            _PTMouseOverConsumer(_PTMouseOver)
        end
    end
end

_PTEvents = function()
    local ev = _PTPumpEvent()
    while ev do
        local result = 0
        if not _PTInputGrabbed then
            if _PTGlobalEventConsumers[ev.type] then
                result = _PTGlobalEventConsumers[ev.type](ev)
            end
        else
            -- Grabbed input mode, intercept events
            if ev.type == "keyDown" and ev.key == "escape" then
                _PTThreadsFastForward["__verb"] = true
            elseif ev.type == "mouseDown" and _PTThreadsActorWait["__verb"] then
                _PTThreadsActorWait["__verb"].talk_millis = _PTGetMillis()
            end
        end
        ev = _PTPumpEvent()
    end
    if not _PTInputGrabbed then
        _PTUpdateMouseOver()
    end
end

_PTRender = function()
    if not _PTCurrentRoom or _PTCurrentRoom._type ~= "PTRoom" then
        return
    end
    if _PTAutoClearScreen then
        _PTClearScreen()
    end
    for i, obj in pairs(_PTCurrentRoom.render_list) do
        frame = PTGetAnimationFrame(obj)
        if frame then
            _PTDrawImage(frame.ptr, obj.x, obj.y)
        end
    end
    for i, obj in pairs(_PTGlobalRenderList) do
        frame = PTGetAnimationFrame(obj)
        if frame then
            _PTDrawImage(frame.ptr, obj.x, obj.y)
        end
    end

    if _PTMouseSprite and not _PTInputGrabbed then
        mouse_x, mouse_y = _PTGetMousePos()
        frame = PTGetAnimationFrame(_PTMouseSprite)
        if frame then
            _PTDrawImage(frame.ptr, mouse_x, mouse_y)
        end
    end
end
