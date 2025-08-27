--- The Perentie scripting API.
-- @module perentie

--- General
-- @section general

--- Get the version string of the Perentie engine.
-- @treturn string Version string.
PTVersion = function()
    return _PTVersion()
end

--- Get the platform which Perentie is running on.
-- @treturn string Platform string. Options are: "dos", "sdl", "web"
PTPlatform = function()
    return _PTPlatform()
end

local _PTActorList = {}
local _PTRoomList = {}
local _PTGlobalRenderList = {}
local _PTCurrentRoom = nil
local _PTOnRoomLoadHandlers = {}
local _PTOnRoomUnloadHandlers = {}
local _PTOnRoomEnterHandlers = {}
local _PTOnRoomExitHandlers = {}

local _PTGameID = nil
local _PTGameVersion = nil
local _PTGameName = nil
--- Set the current game.
-- Required in order to use the save/load system.
-- @tparam string id Identifier code for the game, in reverse domain format. Used to check that a saved state is for the correct game.
-- @tparam string version Version number of the code. Used to state which version of the game created a save state.
-- @tparam string name Human readable name
PTSetGameInfo = function(id, version, name)
    if type(id) ~= "string" then
        error("PTSetGameInfo: expected string for first argument")
    end
    if type(version) ~= "string" then
        error("PTSetGameInfo: expected string for second argument")
    end
    if type(name) ~= "string" then
        error("PTSetGameInfo: expected string for third argument")
    end
    _PTGameID = id
    _PTGameVersion = version
    _PTGameName = name
    _PTSetGameInfo(id, version, name)
end

--- Print a message to the Perentie log.
-- By default, this is only visible if Perentie is started with the --log option.
-- This is a more accessible replacement for Lua's @{print} function, which will only output to the debug console.
-- @tparam string format Format string in @{string.format} syntax.
-- @param ... Arguments for format string.
PTLog = function(...)
    return _PTLog(string.format(...))
end

--- Print an error message to the Perentie log.
-- This will always display.
-- @tparam string format Format string in @{string.format} syntax.
-- @param ... Arguments for format string.
PTLogError = function(...)
    return _PTLogError(string.format(...))
end

--- Get the number of milliseconds elapsed since the engine started.
-- @treturn integer Number of milliseconds.
PTGetMillis = function()
    return _PTGetMillis()
end

--- Get the dimensions of the screen in pixels.
-- @treturn integer Width of the screen.
-- @treturn integer Height of the screen.
PTGetScreenDims = function()
    return _PTGetScreenDims()
end

--- Quit Perentie.
-- @tparam[opt=0] integer retcode Return code.
PTQuit = function(retcode)
    if not retcode then
        retcode = 0
    end
    _PTQuit(retcode)
end

--- Reset Perentie. Clears the scripting engine and restarts.
PTReset = function()
    _PTReset()
end

local _PTVars = {}
--- Return the game's variable store. This will be preserved as part
-- of @{PTLoadState} and @{PTSaveState}.
-- Please use the variable store for simple types only (i.e. strings,
-- numbers, booleans, and tables); you're aiming for the bare minimum of
-- information to describe the current game state.
-- You do not need to store information about the current room and the
-- state of the actors, as these are stored by the engine automatically.
-- You can use a @{PTOnLoadState} or @{PTOnRoomLoad} hook to apply any
-- settings after a game is loaded.
-- @treturn table Table of variables, containing key-value pairs.
PTVars = function()
    return _PTVars
end

--- Save/Load
-- @section saveload

--- Return the file name for a saved state.
-- @tparam integer index Index, between 0 and 999.
-- @treturn string The file name.
PTSaveFileName = function(index)
    if type(index) ~= "number" then
        error(string.format("PTSaveFileName: index must be an integer, not %s", type(index)))
    elseif index < 0 or index > 999 then
        error("PTSaveFileName: index must be between 0 and 999")
    end
    return string.format("SAVE.%03d", index)
end

--- Reset Perentie and load the engine state from a file.
-- Similar to PTReset, this will clear the scripting engine
-- and restart.
-- @tparam[opt=nil] string index Save game index to use. This will be stored in the user's app data path, as provided by @{PTGetAppDataPath}.
PTLoadState = function(index)
    local path = PTSaveFileName(index)
    if not PTGetSaveStateSummary(index) then
        PTLog("PTLoadState: failed to read %s, aborting", path)
    else
        PTLog("PTLoadState: loading state - slot: %d, path: %s", index, path)
        _PTReset(PTSaveFileName(index))
    end
end

--- Get the path for writing app-specific data.
-- @treturn string The app data path (absolute).
PTGetAppDataPath = function()
    return _PTGetAppDataPath()
end

local _PTOnLoadStateHandler = nil
local _PTOnSaveStateHandler = nil
--- Load a saved state as part of the initial engine setup.
-- @local
_PTInitFromStateFile = function(filename)
    PTLog("PTInitFromStateFile: %s", filename)
    local path = filename
    local file, err = io.open(path, "rb")
    if not file then
        error(string.format('PTInitFromStateFile: Unable to open path "%s" for reading: %s', path, tostring(err)))
    end
    local magic = file:read(8)
    if magic ~= "PERENTIE" then
        file:close()
        error(string.format('PTInitFromStateFile: Unrecognised format for file "%s"', path))
    end
    local version = file:read(2)
    local state = {}
    if version == string.char(1, 0) then
        -- version 1: everything in CBOR blob
        state = cbor.decode(file:read("a"))
        file:close()
        if type(state) ~= "table" then
            error(string.format('PTInitFromStateFile: Expected table from file "%s"', path))
        end
    elseif version == string.char(2, 0) then
        -- version 2: basic attributes in chunks, everything else in CBOR blob
        while true do
            local chunk_id = file:read(4)
            if not chunk_id then
                break
            end
            local chunk_size = string.unpack("<I4", file:read(4))
            local chunk = file:read(chunk_size)
            if chunk_id == "PTVR" then
                state.pt_version = chunk
            elseif chunk_id == "GMVR" then
                state.game_version = chunk
            elseif chunk_id == "GMID" then
                state.game_id = chunk
            elseif chunk_id == "NAME" then
                state.name = chunk
            elseif chunk_id == "TIME" then
                state.timestamp = chunk
            elseif chunk_id == "DATA" then
                local data = cbor.decode(chunk)
                if type(data) ~= "table" then
                    error(string.format('PTInitFromStateFile: Expected table from file "%s"', path))
                end
                state.vars = data.vars
                state.actors = data.actors
                state.rooms = data.rooms
                state.current_room = data.current_room
            end
        end
        file:close()
    else
        file:close()
        error(string.format('PTInitFromStateFile: Unsupported format version for file "%s"', path))
    end

    local game_id = state.game_id
    if not game_id then
        error(string.format('PTInitFromStateFile: No game_id found in file "%s"', path))
    elseif game_id ~= _PTGameID then
        error(
            string.format(
                'PTInitFromStateFile: Expected game_id to be %s, but file "%s" has game_id %s',
                _PTGameID,
                path,
                game_id
            )
        )
    end
    if _PTOnLoadStateHandler then
        _PTOnLoadStateHandler(state)
    end
    PTImportState(state)
end

local _PTSaveToStateFile = function(index, state_name)
    local path = PTSaveFileName(index)
    local file = io.open(path, "wb")
    if not file then
        error(string.format('PTSaveToStateFile: Unable to open path "%s" for writing', path))
    end

    local state = PTExportState(state_name)
    if _PTOnSaveStateHandler then
        _PTOnSaveStateHandler(state)
    end

    file:write("PERENTIE") -- magic number
    file:write(string.char(2, 0)) -- format version, little endian

    -- Lift chunk data from state
    local pt_version = state.pt_version
    state.pt_version = nil
    local game_id = state.game_id
    state.game_id = nil
    local game_version = state.game_version
    state.game_version = nil
    local name = state.name
    state.name = nil
    local timestamp = state.timestamp
    state.timestamp = nil

    PTLog("PTSaveStateToFile: writing state - slot: %d, path: %s, name: %s", index, path, state_name)
    file:write("PTVR")
    file:write(string.pack("<I4", #pt_version))
    file:write(pt_version)
    file:write("GMVR")
    file:write(string.pack("<I4", #game_version))
    file:write(game_version)
    file:write("GMID")
    file:write(string.pack("<I4", #game_id))
    file:write(game_id)
    file:write("NAME")
    file:write(string.pack("<I4", #name))
    file:write(name)
    file:write("TIME")
    file:write(string.pack("<I4", #timestamp))
    file:write(timestamp)
    local data = cbor.encode(state)
    file:write("DATA")
    file:write(string.pack("<I4", #data))
    file:write(data)
    file:close()
end

local _PTSaveIndex = nil
local _PTSaveStateName = nil
--- Save the current game state to a file.
-- This is a deferred operation.
-- @tparam integer index Save game index to use. This will be stored in the user's app data path, as provided by @{PTGetAppDataPath}, with the filename provided by @{PTSaveFileName}.
-- @tparam[opt=""] string state_name Name of the saved state. Useful for e.g. listing saved games.
PTSaveState = function(index, state_name)
    if type(index) ~= "number" then
        error("PTSaveState: index must be an integer between 0 and 999")
    end

    if not _PTGameID then
        error("PTSaveState: No game ID defined! First, set it up with PTSetGameInfo()")
    end

    if not state_name then
        state_name = ""
    end

    local path = PTSaveFileName(index)
    local file = io.open(path, "a+b")
    if not file then
        error(string.format('PTSaveState: Unable to open path "%s" for writing', path))
    end
    file:close()

    -- Defer until end of event loop
    _PTSaveIndex = index
    _PTSaveStateName = state_name
end

--- Export the current game state.
-- @tparam string state_name Name of the saved state. Useful for e.g. listing saved games.
-- @treturn table Engine state information payload. When saving to a file with @{PTSaveState}, this data is encoded as CBOR.
PTExportState = function(state_name)
    local room = PTCurrentRoom()
    local vars = {}
    local actors = {}
    local rooms = {}
    for i, obj in pairs(_PTVars) do
        if type(obj) == "userdata" then
            PTLog("PTExportState(): Variable %s was found to be a C binding! This isn't going to work.", tostring(i))
        elseif type(obj) == "table" and obj._type and string.sub(tostring(obj._type), 1, 2) == "PT" then
            PTLog(
                "PTExportState(): Variable %s was found to be a PT type! Please don't stick these in the variable store, they need to be initialised by the game when starting up.",
                tostring(i)
            )
        else
            vars[i] = obj
        end
    end
    for i, obj in pairs(_PTActorList) do
        table.insert(actors, {
            name = obj.name,
            x = obj.x,
            y = obj.y,
            z = obj.z,
            facing = obj.facing,
            room = obj.room and obj.room.name or nil,
        })
    end
    for i, obj in pairs(_PTRoomList) do
        table.insert(rooms, {
            name = obj.name,
            x = obj.x,
            y = obj.y,
            camera_actor = obj.camera_actor and obj.camera_actor.name or nil,
        })
    end
    return {
        pt_version = _PTVersion(),
        game_id = _PTGameID,
        game_version = _PTGameVersion,
        name = state_name,
        timestamp = os.date("%Y-%m-%dT%H:%M:%S"),
        vars = vars,
        actors = actors,
        rooms = rooms,
        current_room = room.name,
    }
end

--- Import game state from a payload.
-- This will overwrite variables in the game's variable store, and update the state of all @{PTRoom}s and @{PTActor}s.
-- @tparam table state Engine state information payload. When loading state from a file with @{PTLoadState}, this data is encoded as CBOR.
PTImportState = function(state)
    if not state or not state.pt_version then
        error("PTImportState: expected a state payload")
    end
    for i, obj in pairs(state.vars) do
        _PTVars[i] = obj
    end
    for _, obj in ipairs(state.actors) do
        if not _PTActorList[obj.name] then
            PTLog("PTImportState: No actor found with name %s, skipping", obj.name)
        else
            local actor = _PTActorList[obj.name]
            if obj.room then
                if _PTRoomList[obj.room] then
                    PTActorSetRoom(actor, _PTRoomList[obj.room], obj.x, obj.y)
                    actor.z = obj.z
                    actor.facing = obj.facing
                else
                    PTLog("PTImportState: No room found with name %s, can't add actor %s to it", obj.room, obj.name)
                end
            else
                actor.x = obj.x
                actor.y = obj.y
                actor.z = obj.z
                actor.facing = obj.facing
            end
        end
    end
    for _, obj in ipairs(state.rooms) do
        if not _PTRoomList[obj.name] then
            PTLog("PTImportState: No room found with name %s, skipping", obj.name)
        else
            local room = _PTRoomList[obj.name]
            room.x = obj.x
            room.y = obj.y
            if obj.camera_actor and _PTActorList[obj.camera_actor] then
                room.camera_actor = _PTActorList[obj.camera_actor]
            end
        end
    end

    if not state.current_room or not _PTRoomList[state.current_room] then
        PTLog("PTImportState: room %s not found", state.current_room)
    else
        local current = PTCurrentRoom()
        local room = _PTRoomList[state.current_room]
        if current and _PTOnRoomUnloadHandlers[current.name] then
            PTLog("PTImportState: calling unload handler for %s", current.name)
            _PTOnRoomUnloadHandlers[current.name]()
        end
        _PTCurrentRoom = room.name
        if room and _PTOnRoomLoadHandlers[room.name] then
            PTLog("PTImportState: calling load handler for %s", room.name)
            _PTOnRoomLoadHandlers[room.name]()
        end
    end
end

--- Get a summary of a single save state file.
-- @tparam integer index Save game index to use. This will be fetched from the user's app data path, as provided by @{PTGetAppDataPath}, with the filename provided by @{PTSaveFileName}.
-- @treturn table A table containing "index", "name" and "timestamp" keys, or nil if the file wasn't found.
PTGetSaveStateSummary = function(index)
    local result = nil
    local path = PTSaveFileName(index)
    local f, _ = io.open(path, "rb")
    if f then
        local magic = f:read(8)
        if magic ~= "PERENTIE" then
            f:close()
            PTLog('PTGetSaveStateSummary: Unrecognised format for file "%s"', path)
            return
        end
        local version = f:read(2)
        if version == string.char(1, 0) then
            -- version 1: everything in CBOR blob
            local content = f:read("a")
            if #content > 0 then
                local state = cbor.decode(content)
                if type(state) == "table" and state.game_id == _PTGameID then
                    result = { index = index, name = state.name, timestamp = state.timestamp }
                else
                    PTLog("PTGetSaveStateSummary: Failed to open %s: payload is wrong", path)
                end
            else
                PTLog("PTGetSaveStateSummary: Failed to open %s: no content", path)
            end
        elseif version == string.char(2, 0) then
            -- version 2: basic attributes in chunks, everything else in CBOR blob
            -- We were running into problems where version 1 would hit the watchdog limit,
            -- as cbor.decode is pretty damn expensive. Having the summary fields stored
            -- as string chunks is a lot cheaper.
            local game_id = nil
            local name = nil
            local timestamp = nil
            while true do
                local chunk_id = f:read(4)
                if not chunk_id then
                    break
                end
                local chunk_size = string.unpack("<I4", f:read(4))

                if chunk_id == "GMID" then
                    game_id = f:read(chunk_size)
                elseif chunk_id == "NAME" then
                    name = f:read(chunk_size)
                elseif chunk_id == "TIME" then
                    timestamp = f:read(chunk_size)
                else
                    f:seek("cur", chunk_size)
                end
            end
            if game_id == _PTGameID then
                result = { index = index, name = name, timestamp = timestamp }
            else
                PTLog("PTGetSaveStateSummary: Failed to open %s: payload is wrong", path)
            end
        else
            PTLog("PTGetSaveStateSummary: Failed to open %s: couldn't find header %s", path, magic)
        end
        f:close()
    end
    return result
end

--- Fetch summaries for all available save state files.
-- @treturn table A list of tables containing "index", "name" and "timestamp" keys; one for each available save state file.
PTListSavedStates = function()
    local results = {}
    for i = 0, 999 do
        local result = PTGetSaveStateSummary(i)
        if result then
            table.insert(results, result)
        end
    end
    return results
end

--- Math
-- @section math

--- Calculate the angle of a 2D direction vector.
-- @tparam number dx Direction vector x coordinate.
-- @tparam number dy Vector y coordinate.
-- @treturn integer Direction in degrees clockwise from north.
PTAngleDirection = function(dx, dy)
    -- dx and dy reversed, so that 0 degrees is at (0, 1)
    -- also dy inverted, as screen coordinates are positive-downwards
    local ang = math.floor(180 * math.atan(dx, -dy) / math.pi)
    if ang < 0 then
        ang = ang + 360
    end
    return ang
end

--- Calculate the delta angle between two directions.
-- @tparam integer src Start direction, in degrees clockwise from north.
-- @tparam integer dest End direction, in degrees clockwise from north.
-- @treturn integer Angle between the two directions, in degrees clockwise.
PTAngleDelta = function(src, dest)
    return ((dest - src + 180) % 360) - 180
end

--- Calculate a direction angle mirrored by a plane.
-- @tparam integer src Start direction, in degrees clockwise from north.
-- @tparam integer plane Plane direction, in degrees clockwise from north.
-- @treturn integer Reflected direction, in degrees clockwise from north.
PTAngleMirror = function(src, plane)
    local delta = PTAngleDelta(plane, src)
    return (plane - delta + 360) % 360
end

--- Return a unique 32-bit hash for a string.
-- @tparam string src String to process.
-- @treturn integer 32-bit hash, encoded as an integer.
PTHash = function(src)
    if type(src) ~= "string" then
        error("PTHash: expected a string")
    end
    return _PTHash(src)
end

--- Generate 1D Perlin simplex noise.
-- @tparam float x X coordinate.
-- @treturn float Noise value, [-1, 1]. 0 on all integer coordinates.
PTSimplexNoise1D = function(x)
    return _PTSimplexNoise1D(x)
end

--- Generate 2D Perlin simplex noise.
-- @tparam float x X coordinate.
-- @tparam float y Y coordinate.
-- @treturn float Noise value, [-1, 1]. 0 on all integer coordinates.
PTSimplexNoise2D = function(x, y)
    return _PTSimplexNoise2D(x, y)
end

--- Generate 3D Perlin simplex noise.
-- @tparam float x X coordinate.
-- @tparam float y Y coordinate.
-- @tparam float z Z coordinate.
-- @treturn float Noise value, [-1, 1]. 0 on all integer coordinates.
PTSimplexNoise3D = function(x, y, z)
    return _PTSimplexNoise1D(x, y, z)
end

--- Generate fractal brownian motion summation of 1D Perlin simplex noise.
-- @tparam float frequency Frequency of the first octave of noise.
-- @tparam float amplitude Amplitude of the first octave of noise.
-- @tparam float lacunarity Frequency multiplier between successive octaves.
-- @tparam float persistence Loss of amplitude between successive octaves.
-- @tparam integer octaves Number of fraction of noise to sum.
-- @tparam float x X coordinate.
-- @treturn float Noise value, [-1, 1]. 0 on all integer coordinates.
PTSimplexFractal1D = function(frequency, amplitude, lacunarity, persistence, octaves, x)
    return _PTSimplexFractal1D(frequency, amplitude, lacunarity, persistence, octaves, x)
end

--- Generate fractal brownian motion summation of 2D Perlin simplex noise.
-- @tparam float frequency Frequency of the first octave of noise.
-- @tparam float amplitude Amplitude of the first octave of noise.
-- @tparam float lacunarity Frequency multiplier between successive octaves.
-- @tparam float persistence Loss of amplitude between successive octaves.
-- @tparam integer octaves Number of fraction of noise to sum.
-- @tparam float x X coordinate.
-- @tparam float y Y coordinate.
-- @treturn float Noise value, [-1, 1]. 0 on all integer coordinates.
PTSimplexFractal2D = function(frequency, amplitude, lacunarity, persistence, octaves, x, y)
    return _PTSimplexFractal2D(frequency, amplitude, lacunarity, persistence, octaves, x, y)
end

--- Generate fractal brownian motion summation of 3D Perlin simplex noise.
-- @tparam float frequency Frequency of the first octave of noise.
-- @tparam float amplitude Amplitude of the first octave of noise.
-- @tparam float lacunarity Frequency multiplier between successive octaves.
-- @tparam float persistence Loss of amplitude between successive octaves.
-- @tparam integer octaves Number of fraction of noise to sum.
-- @tparam float x X coordinate.
-- @tparam float y Y coordinate.
-- @tparam float z Z coordinate.
-- @treturn float Noise value, [-1, 1]. 0 on all integer coordinates.
PTSimplexFractal3D = function(frequency, amplitude, lacunarity, persistence, octaves, x, y, z)
    return _PTSimplexFractal3D(frequency, amplitude, lacunarity, persistence, octaves, x, y, z)
end

local _PTAddToList = function(list, object)
    local exists = false
    for _, obj in ipairs(list) do
        if object == obj then
            exists = true
            break
        end
    end
    if not exists then
        table.insert(list, object)
    end
end

local _PTRemoveFromList = function(list, object)
    for i, obj in ipairs(list) do
        if object == obj then
            table.remove(list, i)
            break
        end
    end
end

--- Callbacks
-- @section callbacks

KEY_FLAG_CTRL = 1
KEY_FLAG_ALT = 2
KEY_FLAG_SHIFT = 4
KEY_FLAG_NUM = 8
KEY_FLAG_CAPS = 16
KEY_FLAG_SCRL = 32

--- Event structure
-- @tfield string _type "PTEvent"
-- @tfield string type Event type code. Options are: "null", "start", "quit", "reset", "keyDown", "keyUp", "mouseMove", "mouseDown", "mouseUp"
-- @tfield[opt=nil] integer status Used by "quit". Exit status code.
-- @tfield[opt=nil] string statePath Used by "reset". Path to the saved state to load.
-- @tfield[opt=nil] string key Used by "keyDown" and "keyUp". Key that has been pressed.
-- @tfield[opt=nil] boolean isRepeat Used by "keyDown". Whether this event is caused by the key repeat rate.
-- @tfield[opt=nil] integer flags Used by "keyDown". Bitmask of modifier keys engaged during the keypress.
-- @tfield[opt=nil] integer x Used by "mouseMove", "mouseDown" and "mouseUp". X position of the mouse, in screen space.
-- @tfield[opt=nil] integer y Used by "mouseMove", "mouseDown" and "mouseUp". Y position of the mouse, in screen space.
-- @table PTEvent

local _PTEventConsumers = {}
--- Set a callback for an event.
-- @tparam string type Event type code. Options are: "null", "start", "quit", "reset", "keyDown", "keyUp", "mouseMove", "mouseDown", "mouseUp"
-- @tparam function callback Function body to call, with the @{PTEvent} object as an argument.
PTOnEvent = function(type, callback)
    _PTEventConsumers[type] = callback
end

local _PTMouseOverConsumer = nil
--- Set a callback for when the mouse moves over a new @{PTActor}/@{PTBackground}/@{PTSprite}.
-- @tparam function callback Function body to call, with the moused-over object as an argument.
PTOnMouseOver = function(callback)
    _PTMouseOverConsumer = callback
end

local _PTRenderFrameConsumer = nil
--- Set a callback for before rendering the current frame to the screen. Useful for animating object positions.
-- @tparam function callback Function body to call.
PTOnRenderFrame = function(callback)
    _PTRenderFrameConsumer = callback
end

local _PTTalkSkipWhileGrabbed = nil
--- Set a callback for when the user indicates they wish to skip a single spoken line (i.e. clicking or hitting"." while the input is grabbed).
-- This is separate to the verb thread, which will always be fast-forwarded.
-- @tparam function callback Function body to call.
PTOnTalkSkipWhileGrabbed = function(callback)
    _PTTalkSkipWhileGrabbed = callback
end

local _PTFastForwardWhileGrabbed = nil
--- Set a callback for when the user indicates they wish to fast-forward a sequence (i.e. hitting Escape while the input is grabbed).
-- This is separate to the verb thread, which will always be fast-forwarded.
-- @tparam function callback Function body to call.
PTOnFastForwardWhileGrabbed = function(callback)
    _PTFastForwardWhileGrabbed = callback
end

--- Set a callback for setting up a particular room.
-- This code will be called whenever @{PTSwitchRoom} is
-- triggered, and also during restoration of a save game.
-- It is recommended to use this callback function to e.g.
-- arrange the room contents based on variables from the @{PTVars}.
-- @tparam string name Name of the room.
-- @tparam function func Function body to call, with an optional argument
-- for context data.
PTOnRoomLoad = function(name, func)
    if type(name) ~= "string" then
        error("PTOnRoomLoad: expected string for first argument")
    end
    if _PTOnRoomLoadHandlers[name] then
        PTLog("PTOnRoomLoad: overwriting handler for %s", name)
    end
    _PTOnRoomLoadHandlers[name] = func
end

--- Set a callback for unloading a particular room.
-- This code will be called whenever @{PTSwitchRoom} is
-- triggered.
-- @tparam string name Name of the room.
-- @tparam function func Function body to call, with an optional argument
-- for context data.
PTOnRoomUnload = function(name, func)
    if type(name) ~= "string" then
        error("PTOnRoomUnload: expected string for first argument")
    end
    if _PTOnRoomUnloadHandlers[name] then
        PTLog("PTOnRoomUnload: overwriting handler for %s", name)
    end
    _PTOnRoomUnloadHandlers[name] = func
end

--- Set a callback for switching to a particular room.
-- This code will only be called during gameplay, i.e. when
-- @{PTSwitchRoom} is triggered. For code that sets up the
-- state of the room, use @{PTOnRoomLoad}.
-- @tparam string name Name of the room.
-- @tparam function func Function body to call, with an optional argument
-- for context data.
PTOnRoomEnter = function(name, func)
    if type(name) ~= "string" then
        error("PTOnRoomEnter: expected string for first argument")
    end
    if _PTOnRoomEnterHandlers[name] then
        PTLog("PTOnRoomEnter: overwriting handler for %s", name)
    end
    _PTOnRoomEnterHandlers[name] = func
end

--- Set a callback for switching away from a room.
-- @tparam string name Name of the room.
-- @tparam function func Function body to call, with an optional argument
-- for context data.
PTOnRoomExit = function(name, func)
    if type(name) ~= "string" then
        error("PTOnRoomExit: expected string for first argument")
    end
    if _PTOnRoomExitHandlers[name] then
        PTLog("PTOnRoomExit: overwriting handler for %s", name)
    end
    _PTOnRoomExitHandlers[name] = func
end

--- Set the callback to run when loading a game state.
-- Perentie will first load all of your game's code, then read the
-- save file, then call this callback with the state contents, then
-- apply that state to the running game.
-- You can use this as a hook to e.g. check for an earlier game version
-- and apply manual variable fixups to the state.
-- @tparam function callback Function body to call. Takes the state object as an argument.
PTOnLoadState = function(callback)
    _PTOnLoadStateHandler = callback
end

--- Set the callback to run when saving a game state.
-- Perentie will first generate a state from the running game,
-- call this callback with the state contents, then write that state to a file.
-- Ideally you shouldn't ever need this.
-- @tparam function callback Function body to call. Takes the state object as an argument.
PTOnSaveState = function(callback)
    _PTOnSaveStateHandler = callback
end

--- Input
-- @section input

--- Get the last recorded mouse position.
-- @treturn integer X coordinate in screen space.
-- @treturn integer Y coordinate in screen space.
PTGetMousePos = function()
    return _PTGetMousePos()
end

local _PTInputGrabbed = false
--- Grab the player's input.
-- For the verb thread, mouse clicks will be remapped to advancing any speech on the screen, and escape will fast-forward.
PTGrabInput = function()
    _PTInputGrabbed = true
end

--- Release the player's input.
PTReleaseInput = function()
    _PTInputGrabbed = false
end

--- Return whether the player's input is grabbed.
-- @treturn boolean Whether the input is grabbed.
PTGetInputGrabbed = function()
    return _PTInputGrabbed
end

--- Return whether the engine has seen a touchscreen input.
-- Perentie will translate touch events to mouse inputs; a normal
-- tap will translate to a left mouse click, a tap held for 500ms
-- will translate to a right mouse click, and any other interaction
-- will count as a left mouse click and drag.
-- This can be used as a check to e.g. enable touch-specific UI elements.
-- Be aware that it is possible for a system to have both touch
-- and mouse inputs.
-- @treturn boolean Whether a touchscreen input has been seen.
PTUsingTouch = function()
    return _PTUsingTouch()
end

local _PTMouseOver = nil
--- Get the current object which the mouse is hovering over
-- @treturn table The object (@{PTActor}/@{PTBackground}/@{PTSprite}), or nil.
PTGetMouseOver = function()
    return _PTMouseOver
end

local _PTUpdateMouseOver = function()
    local mouse_x, mouse_y = PTGetMousePos()
    local room_x, room_y = PTScreenToRoom(mouse_x, mouse_y)
    local room = PTCurrentRoom()
    if not room or room._type ~= "PTRoom" then
        return
    end
    -- Need to iterate through objects in reverse draw order
    for obj, x, y in PTIterObjects(_PTGlobalRenderList, true, false) do
        if obj.collision then
            local frame, flags = PTGetImageFromObject(obj)
            if
                frame
                and PTTestImageCollision(
                    frame,
                    math.floor(mouse_x - x),
                    math.floor(mouse_y - y),
                    flags,
                    frame.collision_mask
                )
            then
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
    for obj, x, y in PTIterObjects(room.render_list, true, false) do
        if obj.collision then
            frame, flags = PTGetImageFromObject(obj)
            if
                frame
                and PTTestImageCollision(
                    frame,
                    math.floor(room_x - x),
                    math.floor(room_y - y),
                    flags,
                    frame.collision_mask
                )
            then
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

--- Walk box
-- @section walkbox

MF_NEW_LEG = 1
MF_IN_LEG = 2
MF_TURN = 4
MF_LAST_LEG = 8

--- Point structure
-- @tfield integer x X coordinate.
-- @tfield integer y Y coordinate.
-- @table PTPoint

--- Create a new point.
-- @tparam integer x X coordinate.
-- @tparam integer y Y coordinate.
-- @treturn PTPoint The new point.
PTPoint = function(x, y)
    return { x = x, y = y }
end

--- Walk box structure.
-- Perentie uses the SCUMM walk box algorithm. All boxes are quads, which can be any convex shape. Two boxes are considered to be connected if they both have a fully horizontal or vertical edge which is coincident; that is, fully or partially overlapping. Boxes cover the pixel area inclusive of the coordinates; it's perfectly acceptable to make a 1px path where each pair of corners is the same.
-- @tfield string _type "PTWalkBox"
-- @tfield PTPoint ul Coordinates of upper-left corner.
-- @tfield PTPoint ur Coordinates of upper-right corner.
-- @tfield PTPoint lr Coordinates of lower-right corner.
-- @tfield PTPoint ll Coordinates of lower-left corner.
-- @tfield integer z Depth coordinate; a higher number renders to the front. Used for setting the depth of PTActors.
-- @tfield[opt=nil] integer id Internal ID of the walk box. Used internally by the box matrix.
-- @tfield[opt=nil] function on_enter Callback for when an actor enters a walkbox, with the walk box and actor as arguments.
-- @table PTWalkBox

--- Create a new walk box.
-- @tparam PTPoint ul Coordinates of upper-left corner.
-- @tparam PTPoint ur Coordinates of upper-right corner.
-- @tparam PTPoint lr Coordinates of lower-right corner.
-- @tparam PTPoint ll Coordinates of lower-left corner.
-- @tparam integer z Depth coordinate; a higher number renders to the front. Used for setting the depth of PTActors.
-- @treturn PTWalkBox The new walk box.
PTWalkBox = function(ul, ur, lr, ll, z)
    return { _type = "PTWalkBox", ul = ul, ur = ur, lr = lr, ll = ll, z = z, id = nil, on_enter = nil }
end

--- Set a callback for when an actor enters a walk box.
-- @tparam PTWalkBox walkbox Walk box to use.
-- @tparam function callback Function body to call, with the walk box and actor as arguments.
PTOnEnterWalkBox = function(walkbox, callback)
    if not walkbox or walkbox._type ~= "PTWalkBox" then
        error("PTOnEnterWalkBox: expected PTWalkBox for first argument")
    end
    walkbox.on_enter = callback
end

local _PTStraightLinesOverlap = function(a1, a2, b1, b2)
    if a1.x == a2.x and b1.x == b2.x and a1.x == b1.x then
        -- vertical line
        local a_start, a_end = math.min(a1.y, a2.y), math.max(a1.y, a2.y)
        local b_start, b_end = math.min(b1.y, b2.y), math.max(b1.y, b2.y)
        return math.max(a_start, b_start) <= math.min(a_end, b_end)
    end
    if a1.y == a2.y and b1.y == b2.y and a1.y == b1.y then
        -- horizontal line
        local a_start, a_end = math.min(a1.x, a2.x), math.max(a1.x, a2.x)
        local b_start, b_end = math.min(b1.x, b2.x), math.max(b1.x, b2.x)
        return math.max(a_start, b_start) <= math.min(a_end, b_end)
    end
    return false
end

--- Generate a list of connections between @{PTWalkBox} objects.
-- Most of the time you won't need to call this yourself; @{PTRoomSetWalkBoxes} will do this for you.
-- @tparam table boxes List of @{PTWalkBox} objects.
-- @treturn table List of index pairs, each describing two directly connected walk boxes in the list.
PTGenLinksFromWalkBoxes = function(boxes)
    local result = {}
    for i = 1, #boxes do
        for j = i + 1, #boxes do
            local a = boxes[i]
            local b = boxes[j]
            if
                _PTStraightLinesOverlap(a.ul, a.ur, b.lr, b.ll) -- sensible comparisons
                or _PTStraightLinesOverlap(a.lr, a.ll, b.ul, b.ur)
                or _PTStraightLinesOverlap(a.ur, a.lr, b.ll, b.ul)
                or _PTStraightLinesOverlap(a.ll, a.ul, b.ur, b.lr)
                or _PTStraightLinesOverlap(a.ul, a.ur, b.ul, b.ur) -- ridiculous comparisons
                or _PTStraightLinesOverlap(a.ul, a.ur, b.ur, b.lr)
                or _PTStraightLinesOverlap(a.ul, a.ur, b.ll, b.ul)
                or _PTStraightLinesOverlap(a.ur, a.lr, b.ul, b.ur)
                or _PTStraightLinesOverlap(a.ur, a.lr, b.ur, b.lr)
                or _PTStraightLinesOverlap(a.ur, a.lr, b.lr, b.ll)
                or _PTStraightLinesOverlap(a.lr, a.ll, b.ur, b.lr)
                or _PTStraightLinesOverlap(a.lr, a.ll, b.lr, b.ll)
                or _PTStraightLinesOverlap(a.lr, a.ll, b.ll, b.ul)
                or _PTStraightLinesOverlap(a.ll, a.ul, b.ul, b.ur)
                or _PTStraightLinesOverlap(a.ll, a.ul, b.lr, b.ll)
                or _PTStraightLinesOverlap(a.ll, a.ul, b.ll, b.ul)
            then
                table.insert(result, { a.id, b.id })
            end
        end
    end
    return result
end

local _PTNewMatrix = function(n)
    local result = {}
    for i = 1, n do
        local inner = {}
        for j = 1, n do
            table.insert(inner, 0)
        end
        table.insert(result, inner)
    end
    return result
end

local _PTCopyMatrix = function(mat)
    local result = {}
    for i = 1, #mat do
        local inner = {}
        for j = 1, #mat[i] do
            table.insert(inner, mat[i][j])
        end
        table.insert(result, inner)
    end
    return result
end

--- Generate a matrix describing the shortest path between walk boxes.
-- Most of the time you won't need to call this yourself; @{PTRoomSetWalkBoxes} will do this for you.
-- @tparam int n Number of walk boxes.
-- @tparam table links List of index pairs, each describing two directly connected walk boxes.
-- @treturn table N x N matrix describing the shortest route between walk boxes; e.g. when starting from box ID i and trying to reach box ID j, result[i][j] is the ID of the next box you need to travel through in order to take the shortest path, or 0 if there is no path.
PTGenWalkBoxMatrix = function(n, links)
    local result = _PTNewMatrix(n)
    for i, link in ipairs(links) do
        result[link[1]][link[2]] = link[2]
        result[link[2]][link[1]] = link[1]
    end

    local modded = true
    while modded do
        modded = false
        local result_prev = result
        result = _PTCopyMatrix(result_prev)
        for a = 1, n do
            for b = 1, n do
                -- if boxes a and b are different, and there's
                -- no existing link between boxes a and b
                if a ~= b and result_prev[a][b] == 0 then
                    for c = 1, n do
                        -- try each existing link from a,
                        -- see if it has a link to b
                        if result_prev[a][c] ~= 0 and result_prev[result_prev[a][c]][b] ~= 0 then
                            result[a][b] = result_prev[a][c]
                            modded = true
                            break
                        end
                    end
                end
            end
        end
    end
    return result
end

local _PTClosestPointOnLine = function(start, finish, target)
    local lxdiff = finish.x - start.x
    local lydiff = finish.y - start.y

    local result = PTPoint(0, 0)

    if finish.x == start.x then
        result = PTPoint(start.x, target.y)
    elseif finish.y == start.y then
        result = PTPoint(target.x, start.y)
    else
        local dist = lxdiff * lxdiff + lydiff * lydiff
        if math.abs(lxdiff) > math.abs(lydiff) then
            local a = start.x * lydiff // lxdiff
            local b = target.x * lxdiff // lydiff
            local c = (a + b - start.y + target.y) * lydiff * lxdiff // dist
            result = PTPoint(c, c * lydiff // lxdiff - a + start.y)
        else
            local a = start.y * lxdiff // lydiff
            local b = target.y * lydiff // lxdiff
            local c = (a + b - start.x + target.x) * lydiff * lxdiff // dist
            result = PTPoint(c * lxdiff // lydiff - a + start.x, c)
        end
    end
    if math.abs(lydiff) < math.abs(lxdiff) then
        if lxdiff > 0 then
            if result.x < start.x then
                result = start
            elseif result.x > finish.x then
                result = finish
            end
        else
            if result.x > start.x then
                result = start
            elseif result.x < finish.x then
                result = finish
            end
        end
    else
        if lydiff > 0 then
            if result.y < start.y then
                result = start
            elseif result.y > finish.y then
                result = finish
            end
        else
            if result.y > start.y then
                result = start
            elseif result.y < finish.y then
                result = finish
            end
        end
    end
    return result
end

local _PTSqrDist = function(a, b)
    return (a.x - b.x) * (a.x - b.x) + (a.y - b.y) * (a.y - b.y)
end

local _PTCompareSlope = function(p1, p2, p3)
    return (p2.y - p1.y) * (p3.x - p1.x) <= (p3.y - p1.y) * (p2.x - p1.x)
end

local _PTCheckPointInBoxBounds = function(point, box)
    -- if the point coordinate is strictly smaller/bigger than
    -- all of the box coordinates, it's outside.
    if point.x < box.ul.x and point.x < box.ur.x and point.x < box.lr.x and point.x < box.ll.x then
        return false
    elseif point.x > box.ul.x and point.x > box.ur.x and point.x > box.lr.x and point.x > box.ll.x then
        return false
    elseif point.y < box.ul.y and point.y < box.ur.y and point.y < box.lr.y and point.y < box.ll.y then
        return false
    elseif point.y > box.ul.y and point.y > box.ur.y and point.y > box.lr.y and point.y > box.ll.y then
        return false
    end

    -- if the box is actually a line segment, add a 2px fuzzy boundary
    if
        (box.ul.x == box.ur.x and box.ul.y == box.ur.y and box.lr.x == box.ll.x and box.lr.y == box.ll.y)
        or (box.ul.x == box.ll.x and box.ul.y == box.ll.y and box.ur.x == box.lr.x and box.ur.y == box.lr.y)
    then
        local tmp = _PTClosestPointOnLine(box.ul, box.lr, point)
        return _PTSqrDist(point, tmp) <= 4
    end

    -- exclude points that are on the wrong side of each of the lines
    if not _PTCompareSlope(box.ul, box.ur, point) then
        return false
    elseif not _PTCompareSlope(box.ur, box.lr, point) then
        return false
    elseif not _PTCompareSlope(box.lr, box.ll, point) then
        return false
    elseif not _PTCompareSlope(box.ll, box.ul, point) then
        return false
    end
    return true
end

local _PTGetClosestPointOnBox = function(point, box)
    local best_dist = 0xfffffff
    local result = PTPoint(0, 0)
    local tmp = _PTClosestPointOnLine(box.ul, box.ur, point)
    local dist = _PTSqrDist(point, tmp)
    if dist < best_dist then
        best_dist = dist
        result = tmp
    end
    tmp = _PTClosestPointOnLine(box.ur, box.lr, point)
    dist = _PTSqrDist(point, tmp)
    if dist < best_dist then
        best_dist = dist
        result = tmp
    end
    tmp = _PTClosestPointOnLine(box.lr, box.ll, point)
    dist = _PTSqrDist(point, tmp)
    if dist < best_dist then
        best_dist = dist
        result = tmp
    end
    tmp = _PTClosestPointOnLine(box.ll, box.ul, point)
    dist = _PTSqrDist(point, tmp)
    if dist < best_dist then
        best_dist = dist
        result = tmp
    end
    return best_dist, result
end

local _PTAdjustPointToBeInBox = function(point, boxes)
    local best_point = PTPoint(0, 0)
    local best_dist = 0xfffffff
    local best_box = nil
    for _, box in ipairs(boxes) do
        if _PTCheckPointInBoxBounds(point, box) then
            best_point = point
            best_dist = 0
            best_box = box
            break
        else
            local dist, target
            dist, target = _PTGetClosestPointOnBox(point, box)
            if dist < best_dist then
                best_point = target
                best_dist = dist
                best_box = box
            end
        end
    end
    if not best_box then
        best_point = PTPoint(point.x, point.y)
    end
    --print(string.format("PTAdjustPointToBeInBox(): point: (%d, %d), best_point: (%d, %d), best_box: %d", point.x, point.y, best_point.x, best_point.y, best_box.id))
    return best_point, best_box
end

local _PTFindPathTowards = function(start_x, start_y, dest_x, dest_y, b1, b2, b3)
    local box1 = PTWalkBox(b1.ul, b1.ur, b1.lr, b1.ll)
    local box2 = PTWalkBox(b2.ul, b2.ur, b2.lr, b2.ll)
    local found_path = PTPoint(0, 0)
    for _ = 1, 4 do
        for _ = 1, 4 do
            --print(string.format("%s %s", inspect(box1, {newline='', indent=''}), inspect(box2, {newline='', indent=''})))
            -- if the top line has the same x coordinate
            if box1.ul.x == box1.ur.x and box1.ul.x == box2.ul.x and box1.ul.x == box2.ur.x then
                local flag = 0
                -- switch y coordinates if not ordered
                if box1.ul.y > box1.ur.y then
                    box1.ul, box1.ur = PTPoint(box1.ul.x, box1.ur.y), PTPoint(box1.ur.x, box1.ul.y)
                    flag = flag | 1
                end
                if box2.ul.y > box2.ur.y then
                    box2.ul, box2.ur = PTPoint(box2.ul.x, box2.ur.y), PTPoint(box2.ur.x, box2.ul.y)
                    flag = flag | 2
                end

                if
                    box1.ul.y > box2.ur.y
                    or box2.ul.y > box1.ur.y
                    or (
                        (box1.ur.y == box2.ul.y or box2.ur.y == box1.ul.y)
                        and box1.ul.y ~= box1.ur.y
                        and box2.ul.y ~= box2.ur.y
                    )
                then
                    -- switch y coordinates back if required
                    if (flag & 1) > 0 then
                        box1.ul, box1.ur = PTPoint(box1.ul.x, box1.ur.y), PTPoint(box1.ur.x, box1.ul.y)
                    end
                    if (flag & 2) > 0 then
                        box2.ul, box2.ur = PTPoint(box2.ul.x, box2.ur.y), PTPoint(box2.ur.x, box2.ul.y)
                    end
                else
                    local pos_y = start_y
                    if b2.id == b3.id then
                        local diff_x = dest_x - start_x
                        local diff_y = dest_y - start_y
                        local box_diff_x = box1.ul.x - start_x
                        if diff_x ~= 0 then
                            diff_y = diff_y * box_diff_x
                            local t = diff_y // diff_x
                            if t == 0 and (diff_y <= 0 or diff_x <= 0) and (diff_y >= 0 or diff_x >= 0) then
                                t = -1
                            end
                            pos_y = start_y + t
                            --print(string.format("pos_y: %d, diff_x: %d, diff_y: %d", pos_y, diff_x, diff_y))
                        end
                    end
                    local q = pos_y
                    if q < box2.ul.y then
                        q = box2.ul.y
                    end
                    if q > box2.ur.y then
                        q = box2.ur.y
                    end
                    if q < box1.ul.y then
                        q = box1.ul.y
                    end
                    if q > box1.ur.y then
                        q = box1.ur.y
                    end
                    if q == pos_y and b2.id == b3.id then
                        return true, found_path
                    end
                    --print(string.format("i: %d, j: %d, box1.ul.x: %d, q: %d", i, j, box1.ul.x, q))
                    found_path = PTPoint(box1.ul.x, q)
                    return false, found_path
                end
            end
            -- if the top line has the same y coordinate
            if box1.ul.y == box1.ur.y and box1.ul.y == box2.ul.y and box1.ul.y == box2.ur.y then
                local flag = 0
                -- switch x coordinates if not ordered
                if box1.ul.x > box1.ur.x then
                    box1.ul, box1.ur = PTPoint(box1.ur.x, box1.ul.y), PTPoint(box1.ul.x, box1.ur.y)
                    flag = flag | 1
                end
                if box2.ul.x > box2.ur.x then
                    box2.ul, box2.ur = PTPoint(box2.ur.x, box2.ul.y), PTPoint(box2.ul.x, box2.ur.y)
                    flag = flag | 2
                end

                if
                    box1.ul.x > box2.ur.x
                    or box2.ul.x > box1.ur.x
                    or (
                        (box1.ur.x == box2.ul.x or box2.ur.x == box1.ul.x)
                        and box1.ul.x ~= box1.ur.x
                        and box2.ul.x ~= box2.ur.x
                    )
                then
                    -- switch x coordinates back if required
                    if (flag & 1) > 0 then
                        box1.ul, box1.ur = PTPoint(box1.ur.x, box1.ul.y), PTPoint(box1.ul.x, box1.ur.y)
                    end
                    if (flag & 2) > 0 then
                        box2.ul, box2.ur = PTPoint(box2.ur.x, box2.ul.y), PTPoint(box2.ul.x, box2.ur.y)
                    end
                else
                    local pos_x = start_x
                    if b2.id == b3.id then
                        local diff_x = dest_x - start_x
                        local diff_y = dest_y - start_y
                        local box_diff_y = box1.ul.y - start_y
                        if diff_y ~= 0 then
                            pos_x = pos_x + (diff_x * box_diff_y // diff_y)
                        end
                        --print(string.format("pos_x: %d, diff_x: %d, diff_y: %d, box_diff_y: %d", pos_y, diff_x, diff_y, box_diff_y))
                    end
                    local q = pos_x
                    if q < box2.ul.x then
                        q = box2.ul.x
                    end
                    if q > box2.ur.x then
                        q = box2.ur.x
                    end
                    if q < box1.ul.x then
                        q = box1.ul.x
                    end
                    if q > box1.ur.x then
                        q = box1.ur.x
                    end
                    if q == pos_x and b2.id == b3.id then
                        return true, found_path
                    end
                    --print(string.format("i: %d, j: %d, q: %d, box1.ul.y: %d", i, j, q, box1.ul.y))
                    found_path = PTPoint(q, box1.ul.y)
                    return false, found_path
                end
            end

            -- rotate box 1
            local tmp = box1.ul
            box1.ul = box1.ur
            box1.ur = box1.lr
            box1.lr = box1.ll
            box1.ll = tmp
        end
        -- rotate box 2
        local tmp = box2.ul
        box2.ul = box2.ur
        box2.ur = box2.lr
        box2.lr = box2.ll
        box2.ll = tmp
    end
    return false, found_path
end

local _PTActorWalkStep = function(actor)
    -- update the walkbox if necessary
    if
        actor.walkdata_curbox
        and actor.walkbox.id ~= actor.walkdata_curbox.id
        and _PTCheckPointInBoxBounds(actor, actor.walkdata_curbox)
    then
        PTActorSetWalkBox(actor, actor.walkdata_curbox)
    end
    local dist_x = math.abs(actor.walkdata_next.x - actor.walkdata_cur.x)
    local dist_y = math.abs(actor.walkdata_next.y - actor.walkdata_cur.y)
    --print(string.format("PTActorWalkStep(): dist_x: %d, dist_y: %d", dist_x, dist_y))
    if math.abs(actor.x - actor.walkdata_cur.x) >= dist_x and math.abs(actor.y - actor.walkdata_cur.y) >= dist_y then
        return false
    end

    -- Lua can't deal with shifting negative numbers, do a divide instead
    local tmp_x = (actor.x * 65536) + actor.walkdata_frac.x + (actor.walkdata_delta_factor.x // 256) * actor.scale_x
    local tmp_y = (actor.y * 65536) + actor.walkdata_frac.y + (actor.walkdata_delta_factor.y // 256) * actor.scale_y
    actor.walkdata_frac = PTPoint(tmp_x & 0xffff, tmp_y & 0xffff)
    actor.x, actor.y = tmp_x // 65536, tmp_y // 65536
    --print(string.format("PTActorWalkStep(): actor.oldpos: (%d, %d), actor.walkdata_frac: (%d, %d), actor.walkdata_delta_factor: (%d, %d)", actor.x, actor.y, actor.walkdata_frac.x, actor.walkdata_frac.y, actor.walkdata_delta_factor.x, actor.walkdata_delta_factor.y))
    --print(string.format("PTActorWalkStep(): tmp_x: %d, tmp_y: %d, actor.pos: (%d, %d)", tmp_x, tmp_y, actor.x, actor.y))
    if math.abs(actor.x - actor.walkdata_cur.x) > dist_x then
        actor.x = actor.walkdata_next.x
    end
    if math.abs(actor.y - actor.walkdata_cur.y) > dist_y then
        actor.y = actor.walkdata_next.y
    end
    --print(string.format("PTActorWalkStep(): actor.walkdata_cur: (%d, %d), actor.walkdata_next: (%d, %d), actor.newpos: (%d, %d)", actor.walkdata_cur.x, actor.walkdata_cur.y, actor.walkdata_next.x, actor.walkdata_next.y, actor.x, actor.y))
    return true
end

local _PTCalcMovementFactor = function(actor, next)
    if actor.x == next.x and actor.y == next.y then
        return false
    end

    local diff_x = next.x - actor.x
    local diff_y = next.y - actor.y
    local delta_y_factor = actor.speed_y * 65536
    if diff_y < 0 then
        delta_y_factor = -delta_y_factor
    end
    local delta_x_factor = delta_y_factor * diff_x
    if diff_y ~= 0 then
        delta_x_factor = delta_x_factor // diff_y
    else
        delta_y_factor = 0
    end

    if math.abs(delta_x_factor // 0x10000) > actor.speed_x then
        delta_x_factor = actor.speed_x * 65536
        if diff_x < 0 then
            delta_x_factor = -delta_x_factor
        end
        delta_y_factor = delta_x_factor * diff_y
        if diff_x ~= 0 then
            delta_y_factor = delta_y_factor // diff_x
        else
            delta_x_factor = 0
        end
    end

    --print(actor.x, actor.y, next.x, next.y, delta_x_factor, delta_y_factor, diff_x, diff_y, actor.speed_x, actor.speed_y)
    actor.walkdata_frac = PTPoint(0, 0)
    actor.walkdata_cur = PTPoint(actor.x, actor.y)
    actor.walkdata_next = next
    actor.walkdata_delta_factor = PTPoint(delta_x_factor, delta_y_factor)
    actor.facing = (math.floor(math.atan(delta_x_factor, -delta_y_factor) * 180 / math.pi) + 360) % 360
    PTSpriteSetAnimation(actor.sprite, actor.anim_walk, actor.facing)
    return _PTActorWalkStep(actor)
end

--- Actors
-- @section actor

--- Actor structure.
-- @tfield string _type "PTActor"
-- @tfield[opt=0] integer x X coordinate in room space.
-- @tfield[opt=0] integer y Y coordinate in room space.
-- @tfield[opt=0] integer z Depth coordinate; a higher number renders to the front.
-- @tfield[opt=true] boolean visible Whether the actor is rendered to the screen.
-- @tfield[opt=nil] PTRoom room The room the actor is located.
-- @tfield[opt=nil] table sprite The @{PTBackground}/@{PTSprite} object used for drawing. Perentie will proxy this; you only need to add the actor object to the rendering list.
-- @tfield[opt=0] integer talk_x X coordinate in actor space for talk text.
-- @tfield[opt=0] integer talk_y Y coordinate in actor space for talk text.
-- @tfield[opt=nil] PTBackground talk_img Handle used by the engine for caching the rendered talk text.
-- @tfield[opt=nil] PTFont talk_font Font to use for rendering talk text.
-- @tfield[opt={ 0xff 0xff 0xff }] table talk_colour Colour to use for rendering talk text.
-- @tfield[opt=0] integer talk_next_wait The millisecond count at which to remove the talk text.
-- @tfield[opt=0] integer facing Direction of the actor; angle in degrees clockwise from north.
-- @tfield[opt="stand"] string anim_stand Name of the sprite animation to use for standing.
-- @tfield[opt="walk"] string anim_walk Name of the sprite animation to use for walking.
-- @tfield[opt="talk"] string anim_talk Name of the sprite animation to use for talking.
-- @tfield[opt=true] boolean use_walkbox Whether the actor snaps to the room's walkboxes.
-- @table PTActor

--- Create a new actor.
-- @tparam string name Name of the actor.
-- @tparam[opt=0] integer x X coordinate in room space.
-- @tparam[opt=0] integer y Y coordinate in room space.
-- @tparam[opt=0] integer z Depth coordinate; a higher number renders to the front.
-- @treturn PTActor The new actor.
PTActor = function(name, x, y, z)
    if not x then
        x = 0
    end
    if not y then
        y = 0
    end
    if not z then
        z = 0
    end
    return {
        _type = "PTActor",
        name = name,
        x = x,
        y = y,
        z = z,
        visible = true,
        room = nil,
        sprite = nil,
        talk_x = 0,
        talk_y = 0,
        talk_img = nil,
        talk_font = nil,
        talk_colour = { 0xff, 0xff, 0xff },
        talk_next_wait = 0,
        walkdata_dest = PTPoint(0, 0),
        walkdata_cur = PTPoint(0, 0),
        walkdata_next = PTPoint(0, 0),
        walkdata_frac = PTPoint(0, 0),
        walkdata_delta_factor = PTPoint(0, 0),
        walkdata_destbox = nil,
        walkdata_curbox = nil,
        walkdata_facing = nil,
        walkbox = nil,
        use_walkbox = true,
        facing = 0,
        scale_x = 0xff,
        scale_y = 0xff,
        speed_x = 8,
        speed_y = 4,
        walk_rate = 12,
        anim_stand = "stand",
        anim_walk = "walk",
        anim_talk = "talk",
        walkdata_next_wait = 0,
        moving = 0,
    }
end

--- Set an actor's walk box.
-- @tparam PTActor actor The actor.
-- @tparam PTWalkBox box Walk box to use.
PTActorSetWalkBox = function(actor, box)
    if not actor or actor._type ~= "PTActor" then
        error("PTActorSetWalkBox: expected PTActor for first argument")
    end
    if not box or box._type ~= "PTWalkBox" then
        error("PTActorSetWalkBox: expected PTWalkBox for second argument")
    end
    if actor.walkbox ~= box then
        actor.walkbox = box
        if box.on_enter then
            box.on_enter(box, actor)
        end
    end
    if box.z and box.z ~= actor.z then
        actor.z = box.z
        -- update the render list order
        PTRoomAddObject(actor.room, nil)
    end
end

local _PTActorUpdate = function(actor, fast_forward)
    if not actor or actor._type ~= "PTActor" then
        error("_PTActorUpdate: expected PTActor for first argument")
    end
    local result = true
    if actor.moving > 0 then
        if PTGetMillis() > actor.walkdata_next_wait then
            PTActorWalk(actor)
            PTSpriteIncrementFrame(actor.sprite)
            actor.walkdata_next_wait = PTGetMillis() + (1000 // actor.walk_rate)
            local room = PTCurrentRoom()
            if room and room._type == "PTRoom" and actor == room.camera_actor then
                room.x, room.y = actor.x, actor.y
            end
        end
        result = false
    end
    if actor.talk_next_wait then
        if actor.talk_next_wait < 0 then
            -- negative talk wait time means wait until click
            result = false
        elseif not fast_forward and _PTGetMillis() < actor.talk_next_wait then
            result = false
        else
            PTActorSilence(actor)
        end
    end

    return result
end

local _PTActorWaitAfterTalk = true
--- Set whether to automatically wait after a PTActor starts talking.
-- If enabled, this means threads can make successive calls to
-- @{PTActorTalk} and the engine will treat them as a conversation;
-- you don't need to explicitly call @{PTWaitForActor} after each one.
-- If you want to do manual conversation timing, disable this feature.
-- Defaults to true.
-- @tparam boolean enable Whether to wait after talking.
PTSetActorWaitAfterTalk = function(enable)
    _PTActorWaitAfterTalk = enable
end

TALK_BASE_DELAY = 1000
TALK_CHAR_DELAY = 85

local _PTTalkBaseDelay = TALK_BASE_DELAY
local _PTTalkCharDelay = TALK_CHAR_DELAY

--- Get the base delay for talking.
-- When a @{PTActor} is talking, this is the fixed amount of time to wait, regardless of text length.
-- @treturn integer The base delay, in milliseconds. Defaults to 1000.
PTGetTalkBaseDelay = function()
    return _PTTalkBaseDelay
end

--- Get the character delay for talking.
-- When a @{PTActor} is talking, this is the variable amount of time to wait, as a multiple of the number of characters in the text.
-- @treturn integer The character delay, in milliseconds. Defaults to 85.
PTGetTalkCharDelay = function()
    return _PTTalkCharDelay
end

--- Set the base delay for talking.
-- @tparam integer delay The base delay, in milliseconds.
PTSetTalkBaseDelay = function(delay)
    _PTTalkBaseDelay = delay
end

--- Set the chracter delay for talking.
-- @tparam integer delay The character delay, in milliseconds.
PTSetTalkCharDelay = function(delay)
    _PTTalkCharDelay = delay
end

--- Make an actor talk.
-- This will trigger the actor's talk animation, as defined in actor.anim_talk.
-- By default, this will wait the thread until the actor finishes talking. You can disable this by calling @{PTSetActorWaitAfterTalk}.
-- @tparam PTActor actor The actor.
-- @tparam string message Message to show on the screen.
-- @tparam[opt=nil] PTFont font Font to use. Defaults to actor.talk_font
-- @tparam[opt=nil] table colour Inner colour; list of 3 8-bit numbers. Defaults to actor.talk_colour.
-- @tparam[opt=nil] integer duration Duration of the message, in milliseconds. By default this varies based on the length of the message; see @{PTGetTalkBaseDelay} and @{PTGetTalkCharDelay}.
PTActorTalk = function(actor, message, font, colour, duration)
    if not actor or actor._type ~= "PTActor" then
        error("PTActorTalk: expected PTActor for first argument")
    end
    if not font then
        font = actor.talk_font
    end
    if not font or font._type ~= "PTFont" then
        PTLog("PTActorTalk: no font argument, or actor has invalid talk_font")
        return
    end
    if not colour then
        colour = actor.talk_colour
    end
    if PTThreadInFastForward() then
        return
    end
    local text = PTText(message, font, 200, "center", colour)
    PTSetImageOriginSimple(text, "bottom")
    local width, height = PTGetImageDims(text)
    local sx, sy = PTRoomToScreen(actor.x + actor.talk_x, actor.y + actor.talk_y)
    local sw, sh = _PTGetScreenDims()

    sx = math.min(math.max(sx, width / 2), sw - width / 2)
    sy = math.min(math.max(sy, height), sh)
    local x, y = PTScreenToRoom(sx, sy)

    if not actor.talk_img then
        actor.talk_img = PTBackground(nil, 0, 0, 20)
    end
    actor.talk_img.x = x
    actor.talk_img.y = y
    actor.talk_img.image = text

    if duration then
        actor.talk_next_wait = _PTGetMillis() + duration
    elseif _PTTalkBaseDelay < 0 or _PTTalkCharDelay < 0 then
        actor.talk_next_wait = -1
    else
        actor.talk_next_wait = _PTGetMillis() + _PTTalkBaseDelay + #message * _PTTalkCharDelay
    end
    PTRoomAddObject(PTCurrentRoom(), actor.talk_img)
    PTSpriteSetAnimation(actor.sprite, actor.anim_talk, actor.facing)
    actor.current_frame = 1
    if _PTActorWaitAfterTalk then
        PTWaitForActor(actor)
    end
end

--- Make an actor stop talking.
-- This will remove any speech bubble, and trigger the actor's stand animation, as defined in actor.anim_stand.
-- @tparam PTActor actor The actor.
PTActorSilence = function(actor)
    if not actor or actor._type ~= "PTActor" then
        error("PTActorSilence: expected PTActor for first argument")
    end
    if actor.talk_img then
        PTRoomRemoveObject(actor.room, actor.talk_img)
        actor.talk_img = nil
    end
    PTSpriteSetAnimation(actor.sprite, actor.anim_stand, actor.facing)
    actor.talk_next_wait = nil
end

--- Sleep the current thread.
-- This is mechanically the same as @{PTSleep}, however it uses the same
-- timer as @{PTActorTalk}, meaning it can be skipped by clicking the mouse.
-- This is useful for e.g. dramatic pauses in dialogue.
-- @tparam PTActor actor The actor.
-- @tparam integer millis Time to wait in milliseconds.
PTActorSleep = function(actor, millis)
    if not actor or actor._type ~= "PTActor" then
        error("PTActorSleep: expected PTActor for first argument")
    end
    if PTThreadInFastForward() then
        return
    end
    actor.talk_next_wait = _PTGetMillis() + millis
    if _PTActorWaitAfterTalk then
        PTWaitForActor(actor)
    end
end

--- Set an actor moving towards a point in the room.
-- @tparam PTActor actor Actor to modify.
-- @tparam integer x X coordinate in room space.
-- @tparam integer y Y coordinate in room space.
-- @tparam[opt=nil] integer facing Direction to face the actor in at the destination.
PTActorSetWalk = function(actor, x, y, facing)
    if not actor or actor._type ~= "PTActor" then
        error("PTActorSetWalk: expected PTActor for first argument")
    end

    if not actor.room or actor.room._type ~= "PTRoom" then
        error("PTActorSetWalk: PTActor isn't assigned to a room")
    end

    local dest = PTPoint(x, y)
    local dest_point, dest_box = dest, nil
    if actor.use_walkbox then
        dest_point, dest_box = _PTAdjustPointToBeInBox(dest, actor.room.boxes)
    end

    actor.walkdata_dest = dest_point
    actor.walkdata_destbox = dest_box
    actor.walkdata_curbox = actor.walkbox
    actor.walkdata_facing = facing
    actor.moving = MF_NEW_LEG
end

--- Make an actor take a single step towards the current walk target.
-- Called by the the event update loop.
-- @tparam PTActor actor Actor to move.
PTActorWalk = function(actor)
    if not actor or actor._type ~= "PTActor" then
        error("PTActorWalk: expected PTActor for first argument")
    end

    if actor.moving == 0 then
        return
    end

    if actor.moving == MF_LAST_LEG and actor.x == actor.walkdata_dest.x and actor.y == actor.walkdata_dest.y then
        actor.moving = 0
        if actor.walkdata_facing then
            actor.facing = actor.walkdata_facing
        end
        PTSpriteSetAnimation(actor.sprite, actor.anim_stand, actor.facing)
        return
    end

    if actor.moving ~= MF_NEW_LEG then
        if _PTActorWalkStep(actor) then
            return
        end
        if actor.moving == MF_LAST_LEG then
            actor.moving = 0
            if actor.walkdata_facing then
                actor.facing = actor.walkdata_facing
            end
            PTSpriteSetAnimation(actor.sprite, actor.anim_stand, actor.facing)
            PTActorSetWalkBox(actor, actor.walkdata_destbox)
            -- turn anim here
        end

        --if actor.moving == MF_TURN then
        -- update_actor_direction
        --end

        PTActorSetWalkBox(actor, actor.walkdata_curbox)
        actor.moving = MF_IN_LEG
    end
    actor.moving = MF_NEW_LEG

    while true do
        if not actor.walkbox and actor.walkdata_destbox then
            PTActorSetWalkBox(actor, actor.walkdata_destbox)
            actor.walkdata_curbox = actor.walkdata_destbox
            break
        end

        local result = true
        local found_path = PTPoint(actor.walkdata_dest.x, actor.walkdata_dest.y)
        if actor.walkbox then
            if actor.walkbox.id == actor.walkdata_destbox.id then
                break
            end

            local next_box = PTRoomGetNextBox(actor.room, actor.walkbox.id, actor.walkdata_destbox.id)
            if not next_box then
                actor.moving = 0
                if actor.walkdata_facing then
                    actor.facing = actor.walkdata_facing
                end
                PTSpriteSetAnimation(actor.sprite, actor.anim_stand, actor.facing)
                return
            end
            actor.walkdata_curbox = next_box
            --print(string.format(
            --    "PTFindPathTowards: (%d, %d) (%d, %d) %d %d %d",
            --    actor.x,
            --    actor.y,
            --    actor.walkdata_dest.x,
            --    actor.walkdata_dest.y,
            --    actor.walkbox.id,
            --    next_box.id,
            --    actor.walkdata_destbox.id
            --    ))

            result, found_path = _PTFindPathTowards(
                actor.x,
                actor.y,
                actor.walkdata_dest.x,
                actor.walkdata_dest.y,
                actor.walkbox,
                next_box,
                actor.walkdata_destbox
            )
            --print(string.format(
            --    "PTFindPathTowards: -> %s, (%d, %d)",
            --    inspect(result),
            --    found_path.x,
            --    found_path.y))

            -- If there's no path to the destination, stop walking.
            -- This feels a bit jank; ideally all walk paths
            -- should terminate at closest spot?
            if actor.walkbox.id == next_box.id then
                break
            end
        end
        PTLog(
            "PTActorWalk: (%d, %d) (%d, %d) %s",
            found_path.x,
            found_path.y,
            actor.walkdata_dest.x,
            actor.walkdata_dest.y,
            tostring(result)
        )
        if result then
            break
        end
        if _PTCalcMovementFactor(actor, found_path) then
            return
        end

        if actor.walkdata_curbox then
            PTActorSetWalkBox(actor, actor.walkdata_curbox)
        end
    end

    actor.moving = MF_LAST_LEG
    _PTCalcMovementFactor(actor, actor.walkdata_dest)
end

--- Add an actor to the engine state.
-- @tparam PTActor actor The actor to add.
PTAddActor = function(actor)
    if not actor or actor._type ~= "PTActor" then
        error("PTAddActor: expected PTActor for first argument")
    end
    if type(actor.name) ~= "string" then
        error("PTAddActor: actor object needs a name")
    end
    _PTActorList[actor.name] = actor
end

--- Remove an actor from the engine state.
-- @tparam PTActor actor The actor to remove.
PTRemoveActor = function(actor)
    if not actor or actor._type ~= "PTActor" then
        error("PTRemoveActor: expected PTActor for first argument")
    end
    if type(actor.name) ~= "string" then
        error("PTRemoveActor: actor object needs a name")
    end

    _PTActorList[actor.name] = nil
end

--- Move an actor into a different room.
-- @tparam PTActor actor The actor to move.
-- @tparam PTRoom room Destination room.
-- @tparam[opt=nil] integer x X position for actor.
-- @tparam[opt=nil] integer y Y position for actor.
-- @tparam[opt=nil] integer z Depth coordinate; a higher number renders to the front.
PTActorSetRoom = function(actor, room, x, y, z)
    if not actor or actor._type ~= "PTActor" then
        error("PTActorSetRoom: expected PTActor for first argument")
    end
    if room and room._type ~= "PTRoom" then
        error("PTActorSetRoom: expected nil or PTRoom for second argument")
    end
    -- If x and y aren't defined, move the actor to the middle of the room.
    -- This likely isn't what you want, but it's better than a crash.
    if not x then
        x = room.width // 2
    end
    if not y then
        y = room.height // 2
    end

    if actor.room then
        PTRoomRemoveObject(actor.room, actor)
        _PTRemoveFromList(actor.room.actors, actor)
    end
    actor.room = room
    if actor.room then
        PTRoomAddObject(actor.room, actor)
        _PTAddToList(actor.room.actors, actor)
    end
    if #actor.room.boxes > 0 then
        local near_point, near_box = PTPoint(x, y), nil
        if actor.use_walkbox then
            near_point, near_box = _PTAdjustPointToBeInBox(near_point, actor.room.boxes)
        end
        actor.x, actor.y, actor.z = near_point.x, near_point.y, z
        if near_box then
            PTActorSetWalkBox(actor, near_box)
        end
    else
        actor.x, actor.y, actor.z = x, y, z
        actor.walkbox = nil
        actor.walkdata_curbox = nil
    end
end

--- Set the current animation to play on an actor's sprite.
-- @tparam PTActor actor The actor to modify.
-- @tparam string name Name of the animation.
-- @tparam[opt=0] integer facing Direction of the animation; angle in degrees clockwise from north.
-- @treturn boolean Whether the current animation was changed.
PTActorSetAnimation = function(actor, name, facing)
    if not actor or actor._type ~= "PTActor" then
        return false
    end

    actor.facing = facing
    return PTSpriteSetAnimation(actor.sprite, name, facing)
end

--- Rendering
-- @section rendering

--- Image structure.
-- @tfield string _type "PTImage"
-- @tfield userdata ptr Pointer to C data.
-- @tfield[opt=true] collision_mask boolean Whether to use the transparency mask for collision detection.
-- @table PTImage

--- Load a new image.
-- @tparam string path Path of the image (must be 8-bit indexed or grayscale PNG).
-- @tparam[opt=0] integer origin_x Origin x coordinate, relative to top-left corner.
-- @tparam[opt=0] integer origin_y Origin y coordinate, relative to top-left corner.
-- @tparam[opt=-1] integer colourkey Palette index to use as colourkey.
-- @tparam[opt=true] boolean collision_mask Whether to use the transparency mask for collision detection.
-- @treturn PTImage The new image.
PTImage = function(path, origin_x, origin_y, colourkey, collision_mask)
    if not origin_x then
        origin_x = 0
    end
    if not origin_y then
        origin_y = 0
    end
    if not colourkey then
        colourkey = -1
    end
    if collision_mask == nil then
        collision_mask = true
    end
    return { _type = "PTImage", ptr = _PTImage(path, origin_x, origin_y, colourkey), collision_mask = collision_mask }
end

--- Load a sequence of images.
-- @tparam string path_format Path of the image (must be 8-bit indexed or grayscale PNG), with %d placeholder for index.
-- @tparam integer start Start index.
-- @tparam integer finish End index (inclusive).
-- @tparam[opt=0] integer origin_x Origin x coordinate, relative to top-left corner.
-- @tparam[opt=0] integer origin_y Origin y coordinate, relative to top-left corner.
-- @treturn table List of new images.
PTImageSequence = function(path_format, start, finish, origin_x, origin_y)
    local result = {}
    for i = start, finish do
        table.insert(result, PTImage(string.format(path_format, i), origin_x, origin_y))
    end
    return result
end

--- 9-slice image reference structure.
-- @tfield string _type "PT9Slice"
-- @tfield PTImage image Image to use as an atlas.
-- @tfield integer x1 X coordinate in image space of left slice plane.
-- @tfield integer y1 Y coordinate in image space of top slice plane.
-- @tfield integer x2 X coordinate in image space of right slice plane.
-- @tfield integer y2 Y coordinate in image space of bottom slice plane.
-- @tfield integer width Width of resulting 9-slice image.
-- @tfield integer height Height of resulting 9-slice image.
-- @table PT9Slice

--- Create a new 9-slice image reference.
-- @tparam PTImage image Image to use as an atlas.
-- @tparam integer x1 X coordinate in image space of left slice plane.
-- @tparam integer y1 Y coordinate in image space of top slice plane.
-- @tparam integer x2 X coordinate in image space of right slice plane.
-- @tparam integer y2 Y coordinate in image space of bottom slice plane.
-- @tparam[opt=0] integer width Width of resulting 9-slice image.
-- @tparam[opt=0] integer height Height of resulting 9-slice image.
-- @treturn PT9Slice The new image reference.
PT9Slice = function(image, x1, y1, x2, y2, width, height)
    if not width then
        width = 0
    end
    if not height then
        height = 0
    end
    return { _type = "PT9Slice", image = image, x1 = x1, y1 = y1, x2 = x2, y2 = y2, width = width, height = height }
end

--- Create a copy of a 9-slice image reference with different dimensions.
-- @tparam PTSlice slice 9-slice image reference to use as a reference.
-- @tparam integer width Width of resulting 9-slice image.
-- @tparam integer height Height of resulting 9-slice image.
-- @treturn PT9Slice The new image reference.
PT9SliceCopy = function(slice, width, height)
    if not slice then
        return nil
    end
    if slice._type ~= "PT9Slice" then
        return nil
    end
    return PT9Slice(slice.image, slice.x1, slice.y1, slice.x2, slice.y2, width, height)
end

FLIP_H = 0x01
FLIP_V = 0x02

--- Get the dimensions of an image.
-- @tparam table image @{PTImage}/@{PT9Slice} to query.
-- @treturn integer Width of the image.
-- @treturn integer Height of the image.
PTGetImageDims = function(image)
    if not image then
        return 0, 0
    end
    if image._type == "PTImage" then
        return _PTGetImageDims(image.ptr)
    elseif image._type == "PT9Slice" then
        return image.width, image.height
    end
    return 0, 0
end

--- Get the origin position of an image.
-- This is the position in image coordinates to use as the origin point;
-- so if the image is e.g. used by a PTBackground object with
-- position (x, y), the image will be rendered with the top-left at position
-- (x - origin_x, y - origin_y).
-- @tparam PTImage image Image to query.
-- @treturn integer X coordinate in image space.
-- @treturn integer Y coordinate in image space.
PTGetImageOrigin = function(image)
    if not image or image._type ~= "PTImage" then
        return 0, 0
    end
    return _PTGetImageOrigin(image.ptr)
end

--- Set the origin position of an image.
-- @tparam PTImage image Image to query.
-- @tparam integer x X coordinate in image space.
-- @tparam integer y Y coordinate in image space.
PTSetImageOrigin = function(image, x, y)
    if not image or image._type ~= "PTImage" then
        return
    end
    _PTSetImageOrigin(image.ptr, x, y)
end

--- Set the origin position of an image.
-- @tparam PTImage image Image to query.
-- @tparam string origin Origin location; one of "top-left", "top", "top-right", "left", "center", "right", "bottom-left", "bottom", "bottom-right"
PTSetImageOriginSimple = function(image, origin)
    if not image or image._type ~= "PTImage" then
        return
    end
    local w, h = _PTGetImageDims(image.ptr)
    local w2, h2 = w / 2, h / 2
    if origin == "top-left" then
        _PTSetImageOrigin(image.ptr, 0, 0)
    elseif origin == "top" then
        _PTSetImageOrigin(image.ptr, w2, 0)
    elseif origin == "top-right" then
        _PTSetImageOrigin(image.ptr, w, 0)
    elseif origin == "left" then
        _PTSetImageOrigin(image.ptr, 0, h2)
    elseif origin == "center" then
        _PTSetImageOrigin(image.ptr, w2, h2)
    elseif origin == "right" then
        _PTSetImageOrigin(image.ptr, w, h2)
    elseif origin == "bottom-left" then
        _PTSetImageOrigin(image.ptr, 0, h)
    elseif origin == "bottom" then
        _PTSetImageOrigin(image.ptr, w2, h)
    elseif origin == "bottom-right" then
        _PTSetImageOrigin(image.ptr, w, h)
    end
end

--- Blit a @{PTImage}/@{PT9Slice} to the screen.
-- Normally not called directly; Perentie will render
-- everything in the display lists managed by @{PTRoomAddObject} and
-- @{PTGlobalAddObject}.
-- @tparam table image Image to blit to the screen.
-- @tparam integer x X coordinate in screen space.
-- @tparam integer y Y coordinate in screen space.
-- @tparam integer flags Flags for rendering the image.
PTDrawImage = function(image, x, y, flags)
    if image then
        if image._type == "PTImage" then
            _PTDrawImage(image.ptr, x, y, flags)
        elseif image._type == "PT9Slice" then
            _PTDraw9Slice(
                image.image.ptr,
                x,
                y,
                flags,
                image.width,
                image.height,
                image.x1,
                image.y1,
                image.x2,
                image.y2
            )
        end
    end
end

--- Perform a collision test for an image.
-- @tparam table image @{PTImage}/@{PT9Slice} to test.
-- @tparam integer x X coordinate in image space.
-- @tparam integer y Y coordinate in image space.
-- @tparam integer flags Flags for rendering the image.
-- @tparam boolean collision_mask If true, test the masked image (i.e. excluding transparent areas), else use the image's bounding box.
PTTestImageCollision = function(image, x, y, flags, collision_mask)
    if not image then
        return false
    end
    if image._type == "PTImage" then
        return _PTImageTestCollision(image.ptr, x, y, flags, collision_mask)
    end
    if image._type == "PT9Slice" and image.image then
        return _PT9SliceTestCollision(
            image.image.ptr,
            x,
            y,
            flags,
            collision_mask,
            image.width,
            image.height,
            image.x1,
            image.y1,
            image.x2,
            image.y2
        )
    end
    return false
end

--- Return the current palette.
-- @treturn table A table containing colours from the current palette. Each colour is a table containing three 8-bit numbers.
PTGetPalette = function()
    return _PTGetPalette()
end

REMAPPER_NONE = 0
REMAPPER_EGA = 1
REMAPPER_CGA0A = 2
REMAPPER_CGA0B = 3
REMAPPER_CGA1A = 4
REMAPPER_CGA1B = 5
REMAPPER_CGA2A = 6
REMAPPER_CGA2B = 7

REMAPPER_MODE_NEAREST = 0
REMAPPER_MODE_HALF = 1
REMAPPER_MODE_QUARTER = 2
REMAPPER_MODE_QUARTER_ALT = 3
REMAPPER_MODE_HALF_NEAREST = 4
REMAPPER_MODE_QUARTER_NEAREST = 5

--- Set the automatic palette remapper to use.
-- This will remove any of the current dithering hints and replace them with the output of the remapper. Subsequent hints can be added.
-- @tparam[opt="none"] string remapper Remapper to use. Choices are: "none", "ega"
-- @tparam[opt="nearest"] string mode Default dither mode to use. Choices are: "nearest", "half", "quarter", "quarter-alt", "half-nearest", "quarter-nearest".
PTSetPaletteRemapper = function(remapper, mode)
    local rm, md = REMAPPER_NONE, REMAPPER_MODE_NEAREST
    if remapper == "none" then
        rm = REMAPPER_NONE
    elseif remapper == "ega" then
        rm = REMAPPER_EGA
    elseif remapper == "cga0a" then
        rm = REMAPPER_CGA0A
    elseif remapper == "cga0b" then
        rm = REMAPPER_CGA0B
    elseif remapper == "cga1a" then
        rm = REMAPPER_CGA1A
    elseif remapper == "cga1b" then
        rm = REMAPPER_CGA1B
    elseif remapper == "cga2a" then
        rm = REMAPPER_CGA2A
    elseif remapper == "cga2b" then
        rm = REMAPPER_CGA2B
    end

    if mode == "nearest" then
        md = REMAPPER_MODE_NEAREST
    elseif mode == "half" then
        md = REMAPPER_MODE_HALF
    elseif mode == "quarter" then
        md = REMAPPER_MODE_QUARTER
    elseif mode == "quarter-alt" then
        md = REMAPPER_MODE_QUARTER_ALT
    elseif mode == "half-nearest" then
        md = REMAPPER_MODE_HALF_NEAREST
    elseif mode == "quarter-nearest" then
        md = REMAPPER_MODE_QUARTER_NEAREST
    end

    if rm ~= -1 then
        _PTSetPaletteRemapper(rm, md)
    end
end

local _PTAutoClearScreen = true
--- Set whether to clear the screen at the start of every frame
-- @tparam boolean val true if the screen is to be cleared, false otherwise.
PTSetAutoClearScreen = function(val)
    _PTAutoClearScreen = val
end

--- Set the overscan colour.
-- For DOS, this is the colour used for the non-drawable area on a CRT/LCD monitor. For SDL, this is the colour used for filling the spare area around the viewport.
-- @tparam table colour Table containing three 8-bit numbers.
PTSetOverscanColour = function(colour)
    return _PTSetOverscanColour(colour)
end

DITHER_NONE = 0
DITHER_FILL_A = 1
DITHER_FILL_B = 2
DITHER_QUARTER = 3
DITHER_QUARTER_ALT = 4
DITHER_HALF = 5

EGA_BLACK = { 0x00, 0x00, 0x00 }
EGA_BLUE = { 0x00, 0x00, 0xaa }
EGA_GREEN = { 0x00, 0xaa, 0x00 }
EGA_CYAN = { 0x00, 0xaa, 0xaa }
EGA_RED = { 0xaa, 0x00, 0x00 }
EGA_MAGENTA = { 0xaa, 0x00, 0xaa }
EGA_BROWN = { 0xaa, 0x55, 0x00 }
EGA_LGRAY = { 0xaa, 0xaa, 0xaa }
EGA_DGRAY = { 0x55, 0x55, 0x55 }
EGA_BRBLUE = { 0x55, 0x55, 0xff }
EGA_BRGREEN = { 0x55, 0xff, 0x55 }
EGA_BRCYAN = { 0x55, 0xff, 0xff }
EGA_BRRED = { 0xff, 0x55, 0x55 }
EGA_BRMAGENTA = { 0xff, 0x55, 0xff }
EGA_BRYELLOW = { 0xff, 0xff, 0x55 }
EGA_WHITE = { 0xff, 0xff, 0xff }

--- Set a dithering hint. Used for remapping a VGA-style palette to a smaller colour range such as EGA or CGA.
-- @tparam table src Source colour, table containing three 8-bit numbers.
-- @tparam string dither_type Type of dithering algorithm to use. Choices are: "none", "fill-a", "fill-b", "quarter", "quarter-alt", "half"
-- @tparam table dest_a Destination mixing colour A, table containing three 8-bit numbers.
-- @tparam table dest_b Destination mixing colour B, table containing three 8-bit numbers.
PTSetDitherHint = function(src, dither_type, dest_a, dest_b)
    local dt = DITHER_NONE
    if dither_type == "none" then
        dt = DITHER_NONE
    elseif dither_type == "fill-a" then
        dt = DITHER_FILL_A
    elseif dither_type == "fill-b" then
        dt = DITHER_FILL_B
    elseif dither_type == "quarter" then
        dt = DITHER_QUARTER
    elseif dither_type == "quarter-alt" then
        dt = DITHER_QUARTER_ALT
    elseif dither_type == "half" then
        dt = DITHER_HALF
    end

    return _PTSetDitherHint(src, dt, dest_a, dest_b)
end

--- Background structure.
-- @tfield string _type "PTBackground"
-- @tfield[opt=0] integer x X coordinate.
-- @tfield[opt=0] integer y Y coordinate.
-- @tfield[opt=0] integer z Depth coordinate; a higher number renders to the front.
-- @tfield[opt=1] float parallax_x X parallax scaling factor.
-- @tfield[opt=1] float parallax_y Y parallax scaling factor.
-- @tfield[opt=false] boolean collision Whether to test this object's sprite mask for collisions; e.g. when updating the current @{PTGetMouseOver} object.
-- @tfield[opt=true] boolean visible Whether to draw this object to the screen.
-- @table PTBackground

--- Create a new background.
-- @tparam PTImage image Image to use.
-- @tparam[opt=0] integer x X coordinate.
-- @tparam[opt=0] integer y Y coordinate.
-- @tparam[opt=0] integer z Depth coordinate; a higher number renders to the front.
-- @tparam[opt=false] boolean collision Whether to test this object's sprite mask for collisions; e.g. when updating the current @{PTGetMouseOver} object.
-- @treturn PTBackground The new background.
PTBackground = function(image, x, y, z, collision)
    if not x then
        x = 0
    end
    if not y then
        y = 0
    end
    if not z then
        z = 0
    end
    if collision == nil then
        collision = false
    end
    return {
        _type = "PTBackground",
        image = image,
        x = x,
        y = y,
        z = z,
        parallax_x = 1,
        parallax_y = 1,
        collision = collision,
        visible = true,
    }
end

--- Animation structure.
-- @tfield string _type "PTAnimation"
-- @tfield string name Name of the animation.
-- @tfield table frames List of @{PTImage} objects; one per frame.
-- @tfield[opt=0] integer rate Frame rate to use for playback.
-- @tfield[opt=0] integer facing Direction of the animation; angle in degrees clockwise from north.
-- @tfield[opt=true] boolean looping Whether to loop the animation when completed.
-- @tfield[opt=0] integer current_frame The current frame in the sequence to display.
-- @tfield[opt=0] integer next_wait The millisecond count at which to show the next frame.
-- @tfield[opt=0] integer flags Flags for rendering the image.
-- @table PTAnimation

--- Create a new animation.
-- @tparam string name Name of the animation.
-- @tparam table frames List of @{PTImage} objects; one per frame.
-- @tparam[opt=0] integer rate Frame rate to use for playback.
-- @tparam[opt=0] integer facing Direction of the animation; angle in degrees clockwise from north.
-- @tparam[opt=false] boolean h_mirror Whether the animation can be mirrored horizontally.
-- @tparam[opt=false] boolean v_mirror Whether the animation can be mirrored vertically.
-- @tparam[opt=true] boolean looping Whether to loop the animation when completed.
-- @treturn PTAnimation The new animation.
PTAnimation = function(name, frames, rate, facing, h_mirror, v_mirror, looping)
    if rate == nil then
        rate = 0
    end
    if facing == nil then
        facing = 0
    end
    if h_mirror == nil then
        h_mirror = false
    end
    if v_mirror == nil then
        v_mirror = false
    end
    if looping == nil then
        looping = true
    end
    return {
        _type = "PTAnimation",
        name = name,
        frames = frames,
        rate = rate,
        facing = facing,
        looping = looping,
        current_frame = 0,
        next_wait = 0,
        h_mirror = h_mirror,
        v_mirror = v_mirror,
        flags = 0,
    }
end

--- Sprite structure.
-- @tfield string _type "PTSprite"
-- @tfield table animations Table of @{PTAnimation} objects.
-- @tfield[opt=0] integer x X coordinate.
-- @tfield[opt=0] integer y Y coordinate.
-- @tfield[opt=0] integer z Depth coordinate; a higher number renders to the front.
-- @tfield[opt=1] float parallax_x X parallax scaling factor.
-- @tfield[opt=1] float parallax_y Y parallax scaling factor.
-- @tfield[opt=nil] integer anim_index Index of the current animation from the animations table.
-- @tfield[opt=0] integer anim_flags Transformation flags to be applied to the frames.
-- @tfield[opt=false] boolean collision Whether to test this object's sprite mask for collisions; e.g. when updating the current @{PTGetMouseOver} object.
-- @tfield[opt=true] boolean visible Whether to draw this object to the screen.
-- @table PTSprite

--- Create a new sprite.
-- @tparam table animations Table of @{PTAnimation} objects.
-- @tparam[opt=0] integer x X coordinate.
-- @tparam[opt=0] integer y Y coordinate.
-- @tparam[opt=0] integer z Depth coordinate; a higher number renders to the front.
-- @treturn PTSprite The new sprite.
PTSprite = function(animations, x, y, z)
    if not x then
        x = 0
    end
    if not y then
        y = 0
    end
    if not z then
        z = 0
    end
    return {
        _type = "PTSprite",
        animations = animations,
        x = x,
        y = y,
        z = z,
        x_parallax = 1,
        y_parallax = 1,
        anim_index = nil,
        anim_flags = 0,
        collision = false,
        visible = true,
    }
end

--- Set the current animation to play on a sprite.
-- @tparam PTSprite sprite The sprite to modify.
-- @tparam string name Name of the animation.
-- @tparam[opt=0] integer facing Direction of the animation; angle in degrees clockwise from north.
-- @treturn boolean Whether the current animation was changed.
PTSpriteSetAnimation = function(sprite, name, facing)
    if not sprite or sprite._type ~= "PTSprite" then
        return false
    end

    if not facing then
        facing = 0
    end

    local best_index = nil
    local best_delta = 0
    local best_flags = 0

    for i, anim in pairs(sprite.animations) do
        if anim.name == name then
            local new_delta = math.abs(PTAngleDelta(facing, anim.facing))
            if not best_index or new_delta < best_delta then
                best_index = i
                best_delta = new_delta
                best_flags = 0
            end
            if anim.h_mirror then
                new_delta = math.abs(PTAngleDelta(facing, PTAngleMirror(anim.facing, 0)))
                if new_delta < best_delta then
                    best_index = i
                    best_delta = new_delta
                    best_flags = FLIP_H
                end
            end
            if anim.v_mirror then
                new_delta = math.abs(PTAngleDelta(facing, PTAngleMirror(anim.facing, 90)))
                if new_delta < best_delta then
                    best_index = i
                    best_delta = new_delta
                    best_flags = FLIP_V
                end
                if anim.h_mirror then
                    new_delta = math.abs(PTAngleDelta(facing, PTAngleMirror(PTAngleMirror(anim.facing, 90), 0)))
                    if new_delta < best_delta then
                        best_index = i
                        best_delta = new_delta
                        best_flags = FLIP_H | FLIP_V
                    end
                end
            end
        end
    end
    if best_index then
        if sprite.anim_index ~= best_index or sprite.anim_flags ~= best_flags then
            --PTLog("PTSpriteSetAnimation name: %s, facing: %d, best_index: %d, best_flags: %d", name, facing,  best_index, best_flags)
            sprite.anim_index = best_index
            sprite.anim_flags = best_flags
            sprite.animations[sprite.anim_index].current_frame = 1
            return true
        end
    end
    return false
end

--- Increment a sprite's current animation frame.
-- This does not take into account the playback rate.
-- @tparam PTSprite object Sprite to update.
PTSpriteIncrementFrame = function(object)
    if object and object._type == "PTSprite" then
        local anim = object.animations[object.anim_index]
        if anim then
            if anim.current_frame == 0 then
                anim.current_frame = 1
            elseif not anim.looping then
                if anim.current_frame < #anim.frames then
                    anim.current_frame = anim.current_frame + 1
                end
            else
                anim.current_frame = (anim.current_frame % #anim.frames) + 1
            end
            --print(string.format("PTSpriteIncrementFrame: %d", anim.current_frame))
        end
    end
end

--- Group structure.
-- @tfield string _type "PTGroup"
-- @tfield table objects List of member objects.
-- @tfield integer x X coordinate.
-- @tfield integer y Y coordinate.
-- @tfield integer z Depth coordinate; a higher number renders to the front.
-- @tfield integer origin_x Origin x coordinate, relative to top-left corner..
-- @tfield integer origin_y Origin y coordinate, relative to top-left corner.
-- @tfield[opt=true] boolean visible Whether to draw this object to the screen.
-- @table PTGroup

--- Create a new group.
-- @tparam table objects List of member objects.
-- @tparam integer x X coordinate.
-- @tparam integer y Y coordinate.
-- @tparam integer z Depth coordinate; a higher number renders to the front.
-- @tparam integer origin_x Origin x coordinate, relative to top-left corner..
-- @tparam integer origin_y Origin y coordinate, relative to top-left corner.
-- @treturn PTGroup The new group.
PTGroup = function(objects, x, y, z, origin_x, origin_y)
    if not objects then
        objects = {}
    end
    table.sort(objects, function(a, b)
        return a.z < b.z
    end)
    if not x then
        x = 0
    end
    if not y then
        y = 0
    end
    if not z then
        z = 0
    end
    if not origin_x then
        origin_x = 0
    end
    if not origin_y then
        origin_y = 0
    end

    return {
        _type = "PTGroup",
        objects = objects,
        x = x,
        y = y,
        z = z,
        origin_x = origin_x,
        origin_y = origin_y,
        visible = true,
    }
end

--- Add a renderable (@{PTActor}/@{PTBackground}/@{PTSprite}/@{PTGroup}) object to a group rendering list.
-- @tparam PTGroup group Group to add object to.
-- @tparam table object Object to add.
PTGroupAddObject = function(group, object)
    if object then
        _PTAddToList(group.objects, object)
    end
    table.sort(group.objects, function(a, b)
        return a.z < b.z
    end)
end

--- Remove a renderable (@{PTActor}/@{PTBackground}/@{PTSprite}/@{PTGroup}) object from a group rendering list.
-- @tparam PTGroup group Group to remove object from.
-- @tparam table object Object to remove.
PTGroupRemoveObject = function(group, object)
    if object then
        _PTRemoveFromList(group.objects, object)
    end
    table.sort(group.objects, function(a, b)
        return a.z < b.z
    end)
end

--- Iterate through a list of renderable (@{PTActor}/@{PTBackground}/@{PTSprite}/@{PTGroup}) objects. PTGroups will be flattened, leaving only PTActor/PTBackground/PTSprite objects with adjusted positions.
-- @tparam table objects List of objects.
-- @tparam[opt=false] boolean reverse Whether to iterate in reverse.
-- @tparam[opt=true] boolean visible_only Whether to only output visible objects.
-- @treturn function An iterator function that returns an object, a x coordinate and a y coordinate.
PTIterObjects = function(objects, reverse, visible_only)
    if reverse == nil then
        reverse = false
    end
    if visible_only == nil then
        visible_only = true
    end
    local i = 1
    local group_iter = nil
    return function()
        local obj
        if group_iter then
            if reverse then
                obj = objects[#objects + 1 - i]
            else
                obj = objects[i]
            end
            local inner, inner_x, inner_y = group_iter()
            if inner then
                return inner,
                    obj.x + inner_x - obj.origin_x + (obj.sx or 0),
                    obj.y + inner_y - obj.origin_y + (obj.sy or 0)
            end
            group_iter = nil
            i = i + 1
        end

        while i <= #objects do
            if reverse then
                obj = objects[#objects + 1 - i]
            else
                obj = objects[i]
            end
            if obj._type == "PTGroup" and (not visible_only or obj.visible) and #obj.objects > 0 then
                group_iter = PTIterObjects(obj.objects, reverse, visible_only)
                local inner, inner_x, inner_y = group_iter()
                if inner then
                    return inner,
                        obj.x + inner_x - obj.origin_x + (obj.sx or 0),
                        obj.y + inner_y - obj.origin_y + (obj.sy or 0)
                end
                group_iter = nil
            elseif obj._type == "PTPanel" and (not visible_only or obj.visible) then
                if #obj.objects > 0 then
                    group_iter = PTIterObjects(obj.objects, reverse, visible_only)
                end
                return obj, obj.x + (obj.sx or 0), obj.y + (obj.sy or 0)
            elseif obj._type == "PTButton" and (not visible_only or obj.visible) then
                if #obj.objects > 0 then
                    group_iter = PTIterObjects(obj.objects, reverse, visible_only)
                end
                return obj, obj.x + (obj.sx or 0), obj.y + (obj.sy or 0)
            elseif obj._type == "PTHorizSlider" and (not visible_only or obj.visible) and #obj.objects > 0 then
                group_iter = PTIterObjects(obj.objects, reverse, visible_only)
                return obj, obj.x + (obj.sx or 0), obj.y + (obj.sy or 0)
            elseif obj._type == "PTActor" or obj._type == "PTBackground" or obj._type == "PTSprite" then
                if not visible_only or obj.visible then
                    i = i + 1
                    return obj, obj.x + (obj.sx or 0), obj.y + (obj.sy or 0)
                end
            end
            i = i + 1
        end
    end
end

--- Fetch the image to use when rendering a @{PTActor}/@{PTBackground}/@{PTSprite} object.
-- @tparam table object The object to query.
-- @treturn table The PTImage or PT9Slice for the current frame.
-- @treturn integer Flags to render the image with.
PTGetImageFromObject = function(object)
    if object then
        if object._type == "PTSprite" then
            local anim = object.animations[object.anim_index]
            if anim then
                if anim.rate == 0 then
                    -- Rate is 0, don't automatically change frames
                    if anim.current_frame == 0 then
                        anim.current_frame = 1
                    end
                elseif anim.current_frame == 0 then
                    anim.current_frame = 1
                    anim.next_wait = PTGetMillis() + (1000 // anim.rate)
                elseif PTGetMillis() > anim.next_wait then
                    if not anim.looping then
                        if anim.current_frame < #anim.frames then
                            anim.current_frame = anim.current_frame + 1
                        end
                    else
                        anim.current_frame = (anim.current_frame % #anim.frames) + 1
                    end
                    anim.next_wait = PTGetMillis() + (1000 // anim.rate)
                end
                return anim.frames[anim.current_frame], object.anim_flags
            end
        elseif object._type == "PTBackground" then
            return object.image, 0
        elseif object._type == "PTActor" then
            return PTGetImageFromObject(object.sprite)
        elseif object._type == "PTPanel" then
            return object.image, 0
        elseif object._type == "PTButton" then
            if object.disabled and object.images.disabled then
                return object.images.disabled, 0
            elseif object.active and object.images.active then
                return object.images.active, 0
            elseif object.hover and object.images.hover then
                return object.images.hover, 0
            elseif object.images.default then
                return object.images.default, 0
            end
        end
    end
    return nil, 0
end

--- Add a renderable (@{PTActor}/@{PTBackground}/@{PTSprite}/@{PTGroup}) object to the global rendering list.
-- @tparam table object Object to add.
PTGlobalAddObject = function(object)
    if object then
        _PTAddToList(_PTGlobalRenderList, object)
    end
    table.sort(_PTGlobalRenderList, function(a, b)
        return a.z < b.z
    end)
end

--- Remove a renderable (@{PTActor}/@{PTBackground}/@{PTSprite}/@{PTGroup}) object from the global rendering list.
-- @tparam table object Object to remove.
PTGlobalRemoveObject = function(object)
    if object then
        _PTRemoveFromList(_PTGlobalRenderList, object)
    end
    table.sort(_PTGlobalRenderList, function(a, b)
        return a.z < b.z
    end)
end

--- Movement
-- @section movement

--- Return a function for linear interpolation.
-- @tparam[opt={0.0 1.0}] table points List of control points to interpolate between.
-- @treturn function Callable that takes a progress value from 0.0-1.0 and returns an output from 0.0-1.0.
PTLinear = function(points)
    if points == nil then
        points = { 0.0, 1.0 }
    end
    return function(progress)
        if #points == 0 then
            return 0.0
        elseif #points == 1 then
            return points[1]
        end
        if progress < 0.0 then
            return points[1]
        elseif progress > 1.0 then
            return points[#points]
        end
        local prog = (#points - 1) * progress
        local lower = math.floor(prog)
        local upper = math.min(#points, lower + 2)
        --PTLog("PTLinear: %s %d %f\n", inspect(points), lower, prog)
        return points[1 + lower] + (points[upper] - points[1 + lower]) * (prog - lower)
    end
end

--- Return a function for a unit cubic bezier curve.
-- Start point is always (0, 0), end point is always (1, 1).
-- @tparam float x1 Curve control point 1 x coordinate.
-- @tparam float y1 Curve control point 1 y coordinate.
-- @tparam float x2 Curve control point 2 x coordinate.
-- @tparam float y2 Curve control point 2 y coordinate.
-- @treturn function Callable that takes a progress value from 0.0-1.0 and returns an output from 0.0-1.0.
PTCubicBezier = function(x1, y1, x2, y2)
    -- Algorithm nicked from Stylo's cubic bezier code, which
    -- was originally based on code from WebKit
    x1 = x1 + 0.0
    y1 = y1 + 0.0
    x2 = x2 + 0.0
    y2 = y2 + 0.0
    --print(string.format("PTCubicBezier(%f, %f, %f, %f)", x1, y1, x2, y2))
    local cx = 3.0 * x1
    local bx = 3.0 * (x2 - x1) - cx
    local ax = 1.0 - cx - bx
    local cy = 3.0 * y1
    local by = 3.0 * (y2 - y1) - cy
    local ay = 1.0 - cy - by

    local f_x = function(t)
        return ((ax * t + bx) * t + cx) * t
    end

    local f_y = function(t)
        return ((ay * t + by) * t + cy) * t
    end

    local f_dx = function(t)
        return (3.0 * ax * t + 2.0 * bx) * t + cx
    end

    local solve_x = function(x, epsilon)
        local t = x
        for _ = 0, 8 do
            local sx = f_x(t)
            if math.abs(sx - x) <= epsilon then
                return t
            end
            local dx = f_dx(t)
            if math.abs(dx) < 1e-6 then
                break
            end
            t = t - ((sx - x) / dx)
        end

        local lo = 0.0
        local hi = 1.0
        t = x
        if t < lo then
            return lo
        elseif t > hi then
            return hi
        end

        while lo < hi do
            local sx = f_x(t)
            if math.abs(sx - x) < epsilon then
                return t
            end
            if x > sx then
                lo = t
            else
                hi = t
            end
            t = ((hi - lo) / 2.0) + lo
        end
        return t
    end

    return function(progress)
        if x1 == y1 and x2 == y2 then
            return progress
        end
        if progress == 0.0 then
            return 0.0
        elseif progress == 1.0 then
            return 1.0
        end
        if progress < 0.0 then
            if x1 > 0.0 then
                return progress * y1 / x1
            elseif y1 == 0.0 and x2 > 0.0 then
                return progress * y2 / x2
            end
            return 0.0
        end
        if progress > 1.0 then
            if x2 < 1.0 then
                return 1.0 + (progress - 1.0) * (y2 - 1.0) / (x2 - 1.0)
            elseif y2 == 1.0 and x1 < 1.0 then
                return 1.0 + (progress - 1.0) * (y1 - 1.0) / (x1 - 1.0)
            end
            return 1.0
        end

        return f_y(solve_x(progress, 1e-6))
    end
end

--- Return a preset timing function.
-- A timing function is a callable that accepts a progress value between 0.0-1.0, and returns an output value between 0.0-1.0.
-- The presets are the same as in the CSS animation-timing-function specification.
-- "linear" - Even speed.
-- "ease" - Increases in speed towards the middle of the animation, slowing back down at the end.
-- "ease-in" - Starts off slowly, with the speed increasing until complete.
-- "ease-out" - Starts off quickly, with the speed decreasing until complete.
-- "ease-in-out" - Starts off slowly, speeds up in the middle, and then the speed decreases until complete.
-- @tparam[opt="linear"] any timing Alias of a builtin function, or a timing function such as one generated from @{PTCubicBezier} or @{PTLinear}.
-- @treturn The timing function.
PTTimingFunction = function(timing)
    if timing == "linear" then
        timing = PTLinear()
    elseif timing == "ease" then
        timing = PTCubicBezier(0.25, 0.1, 0.25, 1.0)
    elseif timing == "ease-in" then
        timing = PTCubicBezier(0.42, 0.0, 1.0, 1.0)
    elseif timing == "ease-out" then
        timing = PTCubicBezier(0.0, 0.0, 0.58, 1.0)
    elseif timing == "ease-in-out" then
        timing = PTCubicBezier(0.42, 0.0, 0.58, 1.0)
    end

    -- Default to linear
    if type(timing) ~= "function" then
        timing = PTLinear()
    end
    return timing
end

--- Movement reference structure.
-- @tfield string _type "PTMoveRef"
-- @tfield function timing Timing function, such as returned from @{PTTimingFunction}.
-- @tfield integer start_time Movement start time, in milliseconds.
-- @tfield integer duration Movement duration, in milliseconds.
-- @tfield table object Any object with "x" and "y" parameters to move.
-- @tfield integer x_a Start X coordinate of the object in room space.
-- @tfield integer y_a Start Y coordinate of the object in room space.
-- @tfield integer x_b Finish X coordinate of the object in room space.
-- @tfield integer y_b Finish Y coordinate of the object in room space.
-- @tfield boolean while_paused Whether to move the object while the game is paused.
-- @table PTMoveRef

--- Create a new movement reference.
-- @tparam table object Any object with "x" and "y" parameters to move.
-- @tparam integer x Finish X coordinate of the object in room space.
-- @tparam integer y Finish Y coordinate of the object in room space.
-- @tparam integer start_time Movement start time, in milliseconds.
-- @tparam integer duration Movement duration, in milliseconds.
-- @tparam function timing Timing function, such as returned from @{PTTimingFunction}.
-- @tparam boolean while_paused Whether to move the object while the game is paused.
-- @treturn PTMoveRef The new movement refrence.
PTMoveRef = function(object, x, y, start_time, duration, timing, while_paused)
    -- timing functions nicked from the CSS animation-timing-function spec
    timing = PTTimingFunction(timing)
    return {
        _type = "PTMoveRef",
        timing = timing,
        start_time = start_time,
        duration = duration,
        object = object,
        x_a = object.x,
        y_a = object.y,
        x_b = x,
        y_b = y,
        while_paused = while_paused,
    }
end

local _PTMoveRefList = {}
--- Move an object smoothly to a destination point.
-- On every rendered frame, Perentie will adjust the "x" and "y" parameters on the target object
-- to move it to the destination, with the speed determined by a duration and a timing function.
-- You can only have one movement instruction per object. If an existing
-- move instruction is found for an object, Perentie will replace it.
-- @tparam table object Any object with "x" and "y" parameters.
-- @tparam integer x Absolute x coordinate to move towards.
-- @tparam integer y Absolute y coordinate to move towards.
-- @tparam integer duration Duration of movement in milliseconds.
-- @tparam any timing Timing function or alias to use, same as timing argument of @{PTTimingFunction}.
-- @tparam[opt=false] boolean while_paused Whether to move the object while the game is paused.
PTMoveObject = function(object, x, y, duration, timing, while_paused)
    for i, obj in ipairs(_PTMoveRefList) do
        if object == obj.object then
            table.remove(_PTMoveRefList, i)
            break
        end
    end
    local moveref = PTMoveRef(object, x, y, PTGetMillis(), duration, timing, while_paused)
    table.insert(_PTMoveRefList, moveref)
end

--- Move an object smoothly to a destination point relative to the object's current position.
-- @tparam table object Any object with "x" and "y" parameters
-- @tparam integer dx X distance relative to the object's current position to move towards.
-- @tparam integer dy Y distance relative to the object's current position to move towards.
-- @tparam integer duration Duration of movement in milliseconds.
-- @tparam any timing Timing function or alias to use, same as timing argument of @{PTTimingFunction}.
-- @tparam[opt=false] boolean while_paused Whether to move the object while the game is paused.
PTMoveObjectRelative = function(object, dx, dy, duration, timing, while_paused)
    PTMoveObject(object, object.x + dx, object.y + dy, duration, timing, while_paused)
end

local _PTObjectIsMoving = function(object, fast_forward)
    for _, obj in ipairs(_PTMoveRefList) do
        if object == obj.object then
            if fast_forward then
                -- fast forward the movement
                obj.start_time = PTGetMillis() - obj.duration
                return false
            else
                return true
            end
        end
    end
    return false
end

--- Check if an object is moving.
-- @tparam table object Any object with "x" and "y" parameters
-- @treturn boolean Whether the object is moving.
PTObjectIsMoving = function(object)
    return _PTObjectIsMoving(object)
end

--- Return a function for shaking using simplex noise.
-- @tparam float x_amplitude X amplitude of the shake.
-- @tparam float y_amplitude Y amplitude of the shake.
-- @tparam float x_frequency X frequency of the shake, in Hz.
-- @tparam float y_frequency Y frequency of the shake, in Hz.
-- @treturn function Callable that takes a time value in millieseconds and returns an (x, y) pair of coordinates.
PTSimplexShake = function(x_amplitude, y_amplitude, x_frequency, y_frequency)
    local simplex_x = math.random() * 2048.0 - 1024.0
    local simplex_y = math.random() * 2048.0 - 1024.0
    local last_offset_x = 0.0
    local last_offset_y = 0.0
    return function(time)
        local shake = 1.0
        local offset_x = x_amplitude * shake * PTSimplexNoise1D((time * x_frequency * 0.001) + simplex_x)
        local offset_y = y_amplitude * shake * PTSimplexNoise1D((time * y_frequency * 0.001) + simplex_y)
        local result_x, result_y = offset_x - last_offset_x, offset_y - last_offset_y
        last_offset_x = offset_x
        last_offset_y = offset_y
        return result_x, result_y
    end
end

--- Shake reference structure
-- @tfield string _type "PTShakeRef"
-- @tfield integer start_time Shake start time, in milliseconds.
-- @tfield integer duration Movement duration, in milliseconds.
-- @tfield table object Any object with "sx" and "sy" parameters to move.
-- @tfield function shake_func Shake function to use, such as the return value of @{PTSimplexShake}.
-- @tparam boolean while_paused Whether to shake the object while the game is paused.
-- @table PTShakeRef

--- Create a new shake reference.
-- @tparam table object Any object with "sx" and "sy" parameters to move.
-- @tparam function shake_func Shake function to use, such as the return value of @{PTSimplexShake}.
-- @tparam integer start_time Shake start time, in milliseconds.
-- @tparam integer duration Movement duration, in milliseconds.
-- @tparam boolean while_paused Whether to shake the object while the game is paused.
PTShakeRef = function(object, shake_func, start_time, duration, while_paused)
    return {
        _type = "PTShakeRef",
        start_time = start_time,
        duration = duration,
        object = object,
        shake_func = shake_func,
        while_paused = while_paused,
    }
end

local _PTShakeRefList = {}
--- Shake an object without changing its position.
-- On every rendered frame, Perentie will adjust the "sx" and "sy" parameters on the target object
-- to adjust the offset from the (x, y) position.
-- You can only have one shake instruction per object. If an existing
-- shake instruction is found for an object, Perentie will replace it.
-- @tparam table object Any object with "sx" and "sy" parameters.
-- @tparam integer duration Duration of shake in milliseconds.
-- @tparam function shake_func Shake function to use, such as the return value of @{PTSimplexShake}.
-- @tparam[opt=false] boolean while_paused Whether to shake the object while the game is paused.
PTShakeObject = function(object, duration, shake_func, while_paused)
    for i, obj in ipairs(_PTShakeRefList) do
        if object == obj.object then
            table.remove(_PTShakeRefList, i)
            break
        end
    end
    local shakeref = PTShakeRef(object, shake_func, PTGetMillis(), duration, while_paused)
    table.insert(_PTShakeRefList, shakeref)
end

local _PTUpdateMoveObject = function()
    local t = PTGetMillis()
    local done = {}
    for i, moveref in ipairs(_PTMoveRefList) do
        if (not _PTGamePaused) or moveref.while_paused then
            local at = (t - moveref.start_time) / moveref.duration
            local a = moveref.timing(at)

            moveref.object.x = moveref.x_a + (moveref.x_b - moveref.x_a) * a
            moveref.object.y = moveref.y_a + (moveref.y_b - moveref.y_a) * a
            --print(string.format("%f,%f", at, a))
            if at >= 1.0 then
                table.insert(done, 1, i)
            end
        end
    end
    for _, i in ipairs(done) do
        table.remove(_PTMoveRefList, i)
    end
end

local _PTUpdateShakeObject = function()
    local t = PTGetMillis()
    local done = {}
    for i, shakeref in ipairs(_PTShakeRefList) do
        if (not _PTGamePaused) or shakeref.while_paused then
            local at = (t - shakeref.start_time) / shakeref.duration

            shakeref.object.sx, shakeref.object.sy = shakeref.shake_func(t - shakeref.start_time)
            --PTLog("(%f, %f) %f", shakeref.object.sx, shakeref.object.sy, at)
            if at >= 1.0 then
                table.insert(done, 1, i)
                shakeref.object.sx = nil
                shakeref.object.sy = nil
            end
        end
    end
    for _, i in ipairs(done) do
        table.remove(_PTShakeRefList, i)
    end
end

--- GUI controls
-- @section controls

-- How do we want to deal with the GUI?
-- We don't need a layout engine. Assume fixed positioning.
-- Assume we give it an x, y, width and height
-- Assume we have three images for button state (default, hover, active, disabled)
-- Assume whatever thing is the focus sits in the middle.
-- We can't use the image collision detection routine?
-- Have GetImageFromObject return the correct image.
-- Main thing is we don't want to have to do boilerplate
-- for rendering or clicking.

-- PTPanel
-- PTButton
--- Has one callback for activation
-- PTSlider
--- Has one callback for value change
-- PTRadio
--- Has one callback for activation
-- PTCheckBox
--- Has one callback for activation
-- PTTextBox
--- Has a callback for edit and a callback for submit

--- Panel structure
-- @tfield string _type "PTPanel"
-- @tfield table image ${PTImage}/${PT9Slice} to use as a background.
-- @tfield integer x X coordinate in screen space.
-- @tfield integer y Y coordinate in screen space.
-- @tfield integer z Depth coordinate; a higher number renders to the front.
-- @tfield integer width Width of panel.
-- @tfield integer height Height of panel.
-- @tfield boolean visible Whether panel is visible.
-- @tfield table objects Child widgets held by panel.
-- @table PTPanel

--- Create a new panel.
-- @tparam table image ${PTImage}/${PT9Slice} to use as a background.
-- @tparam integer x X coordinate in screen space.
-- @tparam integer y Y coordinate in screen space.
-- @tparam integer width Width of panel.
-- @tparam integer height Height of panel.
-- @tparam[opt=true] boolean visible Whether panel is visible.
-- @treturn PTPanel The new panel.
PTPanel = function(image, x, y, width, height, visible)
    if image and image._type == "PT9Slice" then
        image = PT9SliceCopy(image, width, height)
    end
    if visible == nil then
        visible = true
    end
    return {
        _type = "PTPanel",
        image = image,
        x = x,
        y = y,
        z = 0,
        width = width,
        height = height,
        objects = {},
        visible = visible,
        origin_x = 0,
        origin_y = 0,
    }
end

--- Horizontal slider structure.
-- @tfield string _type "PTHorizSlider"
-- @tfield integer value Start value of the slider.
-- @tfield integer min_value Minimum permitted value.
-- @tfield integer max_value Maximum permitted value.
-- @tfield table images Table of ${PTImage}/${PT9Slice} to use. Allowed keys: "default", "hover", "active", "disabled", "track"
-- @tfield integer x X coordinate of widget.
-- @tfield integer y Y coordinate of widget.
-- @tfield integer z Depth coordinate; a higher number renders to the front.
-- @tfield integer width Width of the slider.
-- @tfield integer height Height of the slider.
-- @tfield integer track_size Height of the track image.
-- @tfield integer handle_size Width of the handle image.
-- @tfield boolean hover Whether the widget is currently hovered over.
-- @tfield boolean active Whether the widget is currently active.
-- @tfield boolean disabled Whether the widget is disabled.
-- @tfield boolean visible Whether the widget is visible.
-- @tfield integer origin_x X coordinate of the widget offset.
-- @tfield integer origin_y Y coordinate of the widget offset.
-- @tfield function change_callback Callback to run when slider changes position.
-- @tfield function set_callback Callback to run when the value is set.
-- @tfield table objects Child objects held by widget.
-- @table PTHorizSlider

--- Create a new horizontal slider control.
-- @tparam table images Table of ${PTImage}/${PT9Slice} to use. Allowed keys: "default", "hover", "active", "disabled", "track"
-- @tparam integer x X coordinate.
-- @tparam integer y Y coordinate.
-- @tparam integer width Width of the control.
-- @tparam integer height Height of the control.
-- @tparam integer track_size Height of the track image.
-- @tparam integer handle_size Width of the handle image.
-- @tparam integer value Start value of the slider.
-- @tparam integer min_value Minimum permitted value.
-- @tparam integer max_value Maximum permitted value.
-- @tparam function change_callback Callback to run when slider changes position.
-- @tparam function set_callback Callback to run when the value is set.
-- @treturn PTHorizSlider The new horizontal slider.
PTHorizSlider = function(
    images,
    x,
    y,
    width,
    height,
    track_size,
    handle_size,
    value,
    min_value,
    max_value,
    change_callback,
    set_callback
)
    local target_images = {}
    if images then
        for _, key in ipairs({ "default", "hover", "active", "disabled" }) do
            if images[key] and images[key]._type == "PT9Slice" then
                target_images[key] = PT9SliceCopy(images[key], handle_size, height)
            else
                target_images[key] = images[key]
            end
        end
        if images["track"] and images["track"]._type == "PT9Slice" then
            target_images["track"] = PT9SliceCopy(images["track"], width, track_size)
        else
            target_images["track"] = images["track"]
        end
    end

    local result = {
        _type = "PTHorizSlider",
        value = value,
        min_value = min_value,
        max_value = max_value,
        images = target_images,
        x = x,
        y = y,
        z = 0,
        width = width,
        height = height,
        track_size = track_size,
        handle_size = handle_size,
        hover = false,
        active = false,
        disabled = false,
        visible = true,
        origin_x = 0,
        origin_y = 0,
        change_callback = change_callback,
        set_callback = set_callback,
        objects = {
            PTBackground(target_images["track"], 0, (height - track_size) // 2),
            PTBackground(target_images["default"]),
        },
    }
    PTSliderSetValue(result, value)
    return result
end

--- Convert a slider position to a value.
-- @tparam any slider Slider object to modify.
-- @tparam integer pos New slider position, in screen units.
-- @treturn integer Slider value.
PTSliderPosToValue = function(slider, pos)
    return (
        slider.min_value
        + (((pos * 2 * (slider.max_value - slider.min_value)) // (slider.width - slider.handle_size) + 1) // 2)
    )
end

--- Convert a slider value to a position.
-- @tparam any slider Slider object to modify.
-- @tparam integer value New value to have.
-- @treturn integer Slider position, in screen units.
PTSliderValueToPos = function(slider, value)
    return ((slider.width - slider.handle_size) * (value - slider.min_value) // (slider.max_value - slider.min_value))
end

--- Set a slider to a value.
-- @tparam any slider Slider object to modify.
-- @tparam integer value New value to have.
PTSliderSetValue = function(slider, value)
    slider.value = value
    slider.objects[2].x = PTSliderValueToPos(slider, value)
end

--- Button structure.
-- @tfield string _type "PTButton"
-- @tfield table images Table of ${PTImage}/${PT9Slice} to use. Allowed keys: "default", "hover", "active", "disabled"
-- @tfield integer x X coordinate.
-- @tfield integer y Y coordinate.
-- @tfield integer z Depth coordinate; a higher number renders to the front.
-- @tfield integer width Width of the control.
-- @tfield integer height Height of the control.
-- @tfield table objects Child objects held by widget.
-- @tfield boolean hover Whether the widget is currently hovered over.
-- @tfield boolean active Whether the widget is currently active.
-- @tfield boolean disabled Whether the widget is disabled.
-- @tfield boolean visible Whether the widget is visible.
-- @tfield integer origin_x X coordinate of the widget offset.
-- @tfield integer origin_y Y coordinate of the widget offset.
-- @tfield function callback Callback to run when the button is activated.
-- @table PTButton

--- Create a new button control.
-- @tparam table images Table of ${PTImage}/${PT9Slice} to use. Allowed keys: "default", "hover", "active", "disabled"
-- @tparam integer x X coordinate.
-- @tparam integer y Y coordinate.
-- @tparam integer width Width of the control.
-- @tparam integer height Height of the control.
-- @tparam table objects Child objects held by widget.
-- @tparam function callback Callback to run when the button is activated.
-- @treturn PTButton The new button.
PTButton = function(images, x, y, width, height, objects, callback)
    local target_images = {}
    if images then
        for _, key in ipairs({ "default", "hover", "active", "disabled" }) do
            if images[key] and images[key]._type == "PT9Slice" then
                target_images[key] = PT9SliceCopy(images[key], width, height)
            else
                target_images[key] = images[key]
            end
        end
    end
    return {
        _type = "PTButton",
        images = target_images,
        x = x,
        y = y,
        z = 0,
        width = width,
        height = height,
        objects = objects,
        hover = false,
        active = false,
        disabled = false,
        visible = true,
        origin_x = 0,
        origin_y = 0,
        callback = callback,
    }
end

--- Add a panel to the engine state.
-- @tparam PTPanel panel The panel to add.
local _PTPanelList = {}
PTAddPanel = function(panel)
    if panel and panel._type == "PTPanel" then
        _PTAddToList(_PTPanelList, panel)
    end
    table.sort(_PTPanelList, function(a, b)
        return a.z < b.z
    end)
end

--- Remove a panel from the engine state.
-- @tparam PTPanel panel The panel to remove.
PTRemovePanel = function(panel)
    if not panel or panel._type ~= "PTPanel" then
        error("PTRemovePanel: expected PTPanel for first argument")
    end
    _PTRemoveFromList(_PTPanelList, panel)
    table.sort(_PTPanelList, function(a, b)
        return a.z < b.z
    end)
end

--- Add a renderable (@{PTBackground}/@{PTSprite}/@{PTGroup}) object to the panel rendering list.
-- @tparam PTPanel panel Panel to modify.
-- @tparam table object Object to add.
PTPanelAddObject = function(panel, object)
    if not panel or panel._type ~= "PTPanel" then
        error("PTPanelAddObject: expected PTPanel for first argument")
    end
    _PTAddToList(panel.objects, object)
    table.sort(panel.objects, function(a, b)
        return a.z < b.z
    end)
end

--- Remove a renderable (@{PTBackground}/@{PTSprite}/@{PTGroup}) object from the panel rendering list.
-- @tparam PTPanel panel Panel to modify.
-- @tparam table object Object to add.
PTPanelRemoveObject = function(panel, object)
    if not panel or panel._type ~= "PTPanel" then
        error("PTPanelRemoveObject: expected PTPanel for first argument")
    end
    _PTRemoveFromList(panel.objects, object)
    table.sort(panel.objects, function(a, b)
        return a.z < b.z
    end)
end

local _PTWithinRect = function(x, y, width, height, test_x, test_y)
    return (test_x >= x) and (test_x < (x + width)) and (test_y >= y) and (test_y < (y + height))
end

local _PTGUIActiveObject = nil
local _PTGUIMouseOver = nil
local _PTUpdateGUI = function()
    local has_changed = false
    local mouse_x, mouse_y = PTGetMousePos()
    for _, panel in ipairs(_PTPanelList) do
        if panel.visible then
            for obj, x, y in PTIterObjects({ panel }) do
                if obj._type == "PTButton" then
                    local test = _PTWithinRect(x, y, obj.width, obj.height, mouse_x, mouse_y)
                    obj.hover = test
                    if test then
                        _PTGUIMouseOver = obj
                        has_changed = true
                    end
                    obj.active = test and (obj == _PTGUIActiveObject)
                elseif obj._type == "PTHorizSlider" then
                    local test = _PTWithinRect(x, y, obj.width, obj.height, mouse_x, mouse_y)
                    obj.hover = test
                    if test then
                        _PTGUIMouseOver = obj
                        has_changed = true
                    end
                    obj.active = (obj == _PTGUIActiveObject)
                    if obj.active then
                        local x_rel = math.min(math.max(mouse_x, x), x + obj.width - obj.handle_size) - x
                        local new_value = PTSliderPosToValue(obj, x_rel)
                        --print(x_rel, x_snap, new_value)
                        if obj.value ~= new_value then
                            PTSliderSetValue(obj, new_value)
                            PTStartThread("__gui", obj.change_callback, obj)
                        end
                    end
                end
            end
        end
    end
    if not has_changed then
        _PTGUIMouseOver = nil
    end
end

local _PTGUIEvent = function(ev)
    if ev.type == "mouseDown" and _PTGUIMouseOver then
        if _PTGUIMouseOver._type == "PTButton" or _PTGUIMouseOver._type == "PTHorizSlider" then
            _PTGUIActiveObject = _PTGUIMouseOver
        end
    elseif ev.type == "mouseUp" and _PTGUIActiveObject then
        if _PTGUIActiveObject._type == "PTButton" and _PTGUIActiveObject == _PTGUIMouseOver then
            PTStartThread("__gui", _PTGUIActiveObject.callback, _PTGUIActiveObject)
        elseif _PTGUIActiveObject._type == "PTHorizSlider" then
            PTStartThread("__gui", _PTGUIActiveObject.set_callback, _PTGUIActiveObject)
        end
        _PTGUIActiveObject = nil
    end
end

--- Text
-- @section text

--- Bitmap font structure.
-- @tfield string _type "PTFont"
-- @tfield userdata ptr Pointer to C data.
-- @table PTFont

--- Create a new bitmap font.
-- @tparam string path Path of the bitmap font (must be BMFont V3 binary).
-- @treturn PTFont The new font.
PTFont = function(path)
    return { _type = "PTFont", ptr = _PTFont(path) }
end

--- Create an new image containing rendered text.
-- @tparam string text Unicode text to render.
-- @tparam PTFont font Font object to use.
-- @tparam[opt=200] integer width Width of bounding area in pixels.
-- @tparam[opt="left"] string align Text alignment; one of "left", "center" or "right".
-- @tparam[opt={ 0xff 0xff 0xff }] table colour Inner colour; list of 3 8-bit numbers.
-- @tparam[opt={ 0x00 0x00 0x00 }] table border Border colour; list of 3 8-bit numbers.
-- @treturn PTImage The new image.
PTText = function(text, font, width, align, colour, border)
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
    local brd_r = 0x00
    local brd_g = 0x00
    local brd_b = 0x00
    if border and border[1] then
        brd_r = border[1]
    end
    if border and border[2] then
        brd_g = border[2]
    end
    if border and border[3] then
        brd_b = border[3]
    end

    return { _type = "PTImage", ptr = _PTText(text, font.ptr, width, align_enum, r, g, b, brd_r, brd_g, brd_b) }
end

--- Audio
-- @section audio

--- Wave structure
-- @tfield string _type "PTWave"
-- @tfield userdata ptr Pointer to C data.
-- @table PTWave

--- Load a new wave file.
-- @tparam string path Path of the wave file.
-- @treturn PTWave The new wave.
PTWave = function(path)
    return { _type = "PTWave", ptr = _PTWave(path) }
end

PC_TIMER_FREQ = 1193181

--- Convert a tone frequency to a position on the MIDI pitch scale.
-- The MIDI scale is linear, with an increase of 1 unit per semitone
-- and A440 assigned to 69.
-- Note that the output of this function is not rounded to the nearest
-- integer, nor is it bounded to 0-127, which are required for MIDI usage.
-- @tparam float freq Tone frequency, in Hz.
-- @treturn float MIDI pitch.
PTFreqToMIDI = function(freq)
    if freq == 0 then
        return nil
    end
    return (12 * math.log(freq / 440, 2)) + 69
end

--- Convert a MIDI pitch to a tone frequency.
-- @tparam float midi MIDI pitch.
-- @treturn float Tone frequency, in Hz.
PTMIDIToFreq = function(midi)
    if midi == nil then
        return 0
    end
    return 440 * 2 ^ ((midi - 69) / 12)
end

--- Convert a tone frequency to an Inverse Frequency Sound value.
-- This is the value expected by the PC speaker's square wave generator.
-- @tparam float freq Tone frequency, in Hz.
-- @treturn integer IFS, in PC speaker timing units.
PTFreqToIFS = function(freq)
    if freq == 0 then
        return 0
    end
    return math.max(1, math.min(65534, math.floor(PC_TIMER_FREQ / freq)))
end

--- PC speaker data structure.
-- @tfield string _type "PTPCSpeakerData"
-- @tfield[opt=nil] string name Name of the sample.
-- @tfield userdata ptr Pointer to C data.
-- @table PTPCSpeakerData

--- Create a new PC speaker data buffer.
-- This can be played back using @{PTPCSpeakerPlayData}.
-- @tparam table data List of IFS values, in PC speaker timing units.
-- @tparam[opt=140] integer playback_rate Playback rate for data, in Hz. Defaults to 140, which was the standard rate used by Apogee.
-- @treturn PTPCSpeakerData The new data buffer.
PTPCSpeakerData = function(data, playback_rate)
    if not playback_rate then
        playback_rate = 140
    end
    return { _type = "PTPCSpeakerData", ptr = _PTPCSpeakerData(data, playback_rate) }
end

--- Create a new PC speaker data buffer, using tone frequency values.
-- This can be played back using @{PTPCSpeakerPlayData}.
-- @tparam table data List of tone frequencies, in Hz.
-- @tparam[opt=140] integer playback_rate Playback rate for data, in Hz. Defaults to 140, which was the standard rate used by Apogee.
-- @treturn PTPCSpeakerData The new data buffer.
PTPCSpeakerDataFreq = function(data, playback_rate)
    if not playback_rate then
        playback_rate = 140
    end
    local new_data = {}
    for _, v in ipairs(data) do
        table.insert(new_data, PTFreqToIFS(v))
    end --- Create a new PC speaker data buffer.
    -- This can be played back using @{PTPCSpeakerPlayData}.
    -- @tparam table data List of IFS values, in PC speaker timing units.
    -- @tparam[opt=140] integer playback_rate Playback rate for data, in Hz. Defaults to 140, which is the standard rate used by Apogee.
    -- @treturn PTPCSpeakerData The new data buffer.

    return { _type = "PTPCSpeakerData", ptr = _PTPCSpeakerData(new_data, playback_rate) }
end

-- PTGetWaveSampleRate
-- PTGetWaveSampleCount
-- PTGetWaveLength
-- PTGetWaveChannels
-- PTGetWaveSampleSize

--- Play a square wave tone on the PC speaker.
-- The note will play until it is stopped by a call to @{PTPCSpeakerStop}.
-- @tparam number freq Audio frequency of the tone.
PTPCSpeakerPlayTone = function(freq)
    return _PTPCSpeakerTone(freq)
end

--- Play an audio sample through the PC speaker.
-- This abuses the same timer-driven impulse trick that Access Software's
-- RealSound uses, producing ~6-bit PCM audio.
-- This will not play back if DOS Perentie is running inside Windows.
-- @tparam PTWave wave PTWave to play back. Must be mono unsigned 8-bit samples, either 8000Hz or 16000Hz sample rate.
PTPCSpeakerPlaySample = function(wave)
    if not wave or wave._type ~= "PTWave" then
        error("PTPCSpeakerPlaySample: expected PTWave for first argument")
    end
    _PTPCSpeakerPlaySample(wave.ptr)
end

--- Play a data buffer through the PC speaker.
-- @tparam PTPCSpeakerData data Data buffer to play back.
PTPCSpeakerPlayData = function(data)
    if not data or data._type ~= "PTPCSpeakerData" then
        error("PTPCSpeakerPlayData: expected PTPCSpeakerData for first argument")
    end
    _PTPCSpeakerPlayData(data.ptr)
end

--- Stop all playback through the PC speaker.
PTPCSpeakerStop = function()
    _PTPCSpeakerStop()
end

--- Load a PC speaker sound file in Inverse Frequency Sound format.
-- @tparam string path The path to the file.
-- @treturn table List of @{PTPCSpeakerData} objects for each sound in the file.
PTPCSpeakerLoadIFS = function(path)
    return _PTPCSpeakerLoadIFS(path)
end

--- Load a music file in Reality Adlib Tracker format.
-- @tparam string path The path to the file.
-- @treturn boolean Whether the file was successfully loaded.
PTRadLoad = function(path)
    return _PTRadLoad(path)
end

--- Start playing the music loaded in the RAD player.
PTRadPlay = function()
    _PTRadPlay()
end

--- Stop playing the music loaded in the RAD player.
PTRadStop = function()
    _PTRadStop()
end

--- Get the path of the currently playing file in the RAD player.
-- @treturn string Path of the current playing RAD file.
PTRadGetPath = function()
    return _PTRadGetPath()
end

--- Get the master volume of the RAD player.
-- @treturn integer Volume, ranging from 0 to 255.
PTRadGetVolume = function()
    return _PTRadGetVolume()
end

--- Set the master volume of the RAD player.
-- @tparam integer volume Volume, ranging from 0 to 255.
PTRadSetVolume = function(volume)
    _PTRadSetVolume(volume)
end

--- Get the playback head position of the RAD player.
-- @treturn integer Order list index.
-- @treturn integer Line of the pattern.
PTRadGetPosition = function()
    return _PTRadGetPosition()
end

--- Set the playback head position of the RAD player.
-- @tparam[opt=0] integer order Order list index.
-- @tparam[opt=0] integer line Line of the pattern.
PTRadSetPosition = function(order, line)
    if order == nil or order < 0 then
        order = 0
    end
    if line == nil or line < 0 then
        line = 0
    end
    _PTRadSetPosition(order, line)
end

--- Start playing the music loaded in the RAD player with a volume fade-in.
-- @tparam integer duration Duration of fade, in milliseconds.
PTRadFadeIn = function(duration)
    _PTRadFadeIn(duration)
end

--- Stop playing the music loaded in the RAD player with a volume fade-out.
-- @tparam integer duration Duration of fade, in milliseconds.
PTRadFadeOut = function(duration)
    _PTRadFadeOut(duration)
end

--- Threading
-- @section threading

local _PTThreads = {}
local _PTThreadsSleepUntil = {}
local _PTThreadsActorWait = {}
local _PTThreadsRoomWait = {}
local _PTThreadsMoveObjectWait = {}
local _PTThreadsAnimationWait = {}
local _PTThreadsFastForward = {}

--- Start a function in a new thread.
-- Perentie runs threads with cooperative multitasking; that is,
-- a long-running thread must use a sleep function like @{PTSleep}
-- to yield control back to the engine.
-- @tparam string name Name of the thread. Must be unique.
-- @tparam function func Function to run.
-- @param ... Arguments to pass to the function.
PTStartThread = function(name, func, ...)
    if _PTThreads[name] then
        error(string.format("PTStartThread(): thread named %s exists", name))
    end
    if not func then
        return
    end
    local args = { ... }
    if #args > 0 then
        _PTThreads[name] = coroutine.create(function()
            func(table.unpack(args))
        end)
    else
        _PTThreads[name] = coroutine.create(function()
            func()
        end)
    end
end

--- Stop a running thread.
-- @tparam string name Name of the thread.
-- @tparam boolean ignore_self If true, ignore if this is the currently running thread.
-- @tparam boolean ignore_missing If true, ignore if this thread isn't running.
PTStopThread = function(name, ignore_self, ignore_missing)
    if not _PTThreads[name] then
        if not ignore_missing then
            error(string.format("PTStopThread(): thread named %s doesn't exist", name))
        else
            return
        end
    end
    if ignore_self then
        local thread, _ = coroutine.running()
        if _PTThreads[name] == thread then
            return
        end
    end

    coroutine.close(_PTThreads[name])
    _PTThreads[name] = nil
    _PTThreadsSleepUntil[name] = nil
    _PTThreadsActorWait[name] = nil
    _PTThreadsRoomWait[name] = nil
    _PTThreadsMoveObjectWait[name] = nil
    _PTThreadsAnimationWait[name] = nil
    _PTThreadsFastForward[name] = nil
end

--- Fast forward the current thread.
-- This will cause the engine to skip all sleep/wait instructions.
-- @tparam[opt=true] boolean enabled Whether to enable or disable fast forward.
PTFastForward = function(enabled)
    if enabled == nil then
        enabled = true
    end
    local thread, _ = coroutine.running()
    for k, v in pairs(_PTThreads) do
        if v == thread then
            _PTThreadsFastForward[k] = enabled
            return
        end
    end
    error("PTFastForward(): thread not found")
end

--- Fast forward the named thread.
-- This will cause the engine to skip all sleep/wait instructions.
-- @tparam string name Name of the thread.
-- @tparam boolean ignore_missing If true, ignore if this thread isn't running.
-- @tparam[opt=true] boolean enabled Whether to enable or disable fast forward.
PTFastForwardThread = function(name, ignore_missing, enabled)
    if enabled == nil then
        enabled = true
    end
    if not _PTThreads[name] then
        if not ignore_missing then
            error(string.format("PTFastForwardThread(): thread named %s doesn't exist", name))
        else
            return
        end
    end
    _PTThreadsFastForward[name] = enabled
end

--- Perform a talk skip on the current thread.
-- If the thread is still waiting for an actor, the engine will skip the wait.
PTTalkSkip = function()
    local thread, _ = coroutine.running()
    for k, v in pairs(_PTThreads) do
        if v == thread then
            if _PTThreadsActorWait[k] then
                _PTThreadsActorWait[k].talk_next_wait = _PTGetMillis()
            end
            if _PTThreadsRoomWait[k] then
                _PTThreadsRoomWait[k].talk_next_wait = _PTGetMillis()
            end
            return
        end
    end
    error("PTTalkSkip(): thread not found")
end

--- Perform a talk skip on the named thread.
-- If the thread is still waiting for an actor, the engine will skip the wait.
-- @tparam string name Name of the thread.
-- @tparam boolean ignore_missing If true, ignore if this thread isn't running.
PTTalkSkipThread = function(name, ignore_missing)
    if not _PTThreads[name] then
        if not ignore_missing then
            error(string.format("PTTalkSkipThread(): thread named %s doesn't exist", name))
        else
            return
        end
    end
    if _PTThreadsActorWait[name] then
        _PTThreadsActorWait[name].talk_next_wait = _PTGetMillis()
    end
    if _PTThreadsRoomWait[name] then
        _PTThreadsRoomWait[name].talk_next_wait = _PTGetMillis()
    end
end

--- Check whether a thread is in the fast forward state.
-- @tparam[opt=nil] string name Name of the thread. Defaults to the current execution context.
-- @treturn boolean Whether the thread is in the fast forward state.
PTThreadInFastForward = function(name)
    local thread, _ = coroutine.running()
    for k, v in pairs(_PTThreads) do
        if v == thread then
            return _PTThreadsFastForward[k]
        end
    end
    return false
end

--- Check whether a thread is running.
-- @tparam[opt=nil] string name Name of the thread. Defaults to the current execution context.
-- @treturn boolean Whether the thread exists.
PTThreadExists = function(name)
    if name then
        return _PTThreads[name] ~= nil
    end
    local thread, _ = coroutine.running()
    for _, v in pairs(_PTThreads) do
        if v == thread then
            return true
        end
    end
    return false
end

--- Sleep the current thread.
-- Perentie uses co-operative multitasking; for long-running background
-- tasks it is important to call a wait or sleep function whenever possible,
-- even with a delay of 0.
-- Not doing so will cause the engine to freeze up, until the thread is
-- aborted early by the watchdog.
-- @tparam integer millis Time to wait in milliseconds.
PTSleep = function(millis)
    if type(millis) ~= "number" then
        error(string.format("PTSleep(): argument must be an integer"))
    end
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

--- Sleep the current thread until an actor finishes the action in progress.
-- @tparam PTActor actor The @{PTActor} to wait for.
PTWaitForActor = function(actor)
    if type(actor) ~= "table" or actor._type ~= "PTActor" then
        error(string.format("PTWaitForActor(): argument must be a PTActor"))
    end
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

--- Sleep the current thread until an object finishes moving.
-- @tparam table object The @{PTActor}/@{PTBackground}/@{PTSprite}/@{PTGroup} to wait for.
PTWaitForMoveObject = function(object)
    local thread, _ = coroutine.running()
    for k, v in pairs(_PTThreads) do
        if v == thread then
            _PTThreadsMoveObjectWait[k] = object
            coroutine.yield()
            return
        end
    end
    error(string.format("PTWaitForMoveObject(): thread not found"))
end

--- Sleep the current thread until a PTAnimation reaches the end.
-- @tparam PTAnimation animation The animation to wait for.
PTWaitForAnimation = function(animation)
    local thread, _ = coroutine.running()
    for k, v in pairs(_PTThreads) do
        if v == thread then
            _PTThreadsAnimationWait[k] = animation
            coroutine.yield()
            return
        end
    end
    error(string.format("PTWaitForAnimation(): thread not found"))
end

local _PTWatchdogEnabled = true
--- Toggle the use of the watchdog to abort threads that take too long.
-- Enabled by default.
-- @tparam boolean enable Whether to enable the watchdog.
PTSetWatchdog = function(enable)
    _PTWatchdogEnabled = enable
end

local _PTWatchdogLimit = 10000
--- Set the number of Lua instructions that need to elapse without a sleep
-- before the watchdog aborts a thread.
-- Defaults to 10000.
-- @tparam integer count Number of instructions.
PTSetWatchdogLimit = function(count)
    _PTWatchdogLimit = count
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

--- Hook callback for when a thread runs too many instructions
-- without sleeping. Throws an error.
-- @tparam string event Event provided by the debug layer. Should always be "count".
local _PTWatchdog = function(event)
    if event == "count" then
        local info = debug.getinfo(2, "Sl")
        error(string.format("PTWatchdog(): woof! woooooff!!! %s:%d took too long", info.source, info.currentline))
    end
end

--- Rooms
-- @section room

--- Room structure.
-- @table PTRoom
-- @tfield string type "PTRoom"
-- @tfield string name Name of the room.
-- @tfield integer width Width of the room in pixels.
-- @tfield integer height Height of the room in pixels.
-- @tfield integer x X coordinate of camera in room space.
-- @tfield integer y Y coordinate of camera in room space.
-- @tfield integer origin_x X coordinate of the camera offset in screen space.
-- @tfield integer origin_y Y coordinate of the camera offset in screen space.
-- @tfield table render_list List of renderable (@{PTActor}/@{PTBackground}/@{PTSprite}/@{PTGroup}) objects in the room.
-- @tfield table boxes List of @{PTWalkBox} objects which make up the room's walkable area.
-- @tfield table box_links List of box ID pairs, each describing two directly connected walk boxes.
-- @tfield table box_matrix N x N matrix describing the shortest route between walk boxes; e.g. when starting from box ID i and trying to reach box ID j, box_matrix[i][j] is the ID of the next box you need to travel through in order to take the shortest path, or 0 if there is no path.
-- @tfield PTActor camera_actor Actor to follow with the room camera.

--- Create a new room.
-- @tparam string name Name of the room.
-- @tparam integer width Width of the room in pixels.
-- @tparam integer height Height of the room in pixels.
-- @treturn PTRoom The new room.
PTRoom = function(name, width, height)
    local sw, sh = _PTGetScreenDims()
    return {
        _type = "PTRoom",
        name = name,
        width = width,
        height = height,
        x = 0,
        y = 0,
        origin_x = sw // 2,
        origin_y = sh // 2,
        render_list = {},
        boxes = {},
        box_links = {},
        box_matrix = { {} },
        actors = {},
        camera_actor = nil,
    }
end

--- Add a renderable (@{PTActor}/@{PTBackground}/@{PTSprite}/@{PTGroup}) object to the room rendering list.
-- @tparam PTRoom room Room to modify.
-- @tparam table object Object to add.
PTRoomAddObject = function(room, object)
    if not room or room._type ~= "PTRoom" then
        error("PTRoomAddObject: expected PTRoom for first argument")
        return
    end
    if object then
        _PTAddToList(room.render_list, object)
    end
    table.sort(room.render_list, function(a, b)
        return a.z < b.z
    end)
end

--- Remove a renderable (@{PTActor}/@{PTBackground}/@{PTSprite}/@{PTGroup}) object from the room rendering list.
-- @tparam PTRoom room Room to modify.
-- @tparam table object Object to remove.
PTRoomRemoveObject = function(room, object)
    if not room or room._type ~= "PTRoom" then
        error("PTRoomRemoveObject: expected PTRoom for first argument")
    end
    if object then
        _PTRemoveFromList(room.render_list, object)
    end
    table.sort(room.render_list, function(a, b)
        return a.z < b.z
    end)
end

--- Set the walk boxes for a room.
-- This will replace all existing walk boxes, and regenerate the box links and box matrix for the room.
-- @tparam PTRoom room The room to modify.
-- @tparam table boxes A list of @{PTWalkBox} objects.
PTRoomSetWalkBoxes = function(room, boxes)
    if not room or room._type ~= "PTRoom" then
        error("PTRoomSetWalkBoxes: expected PTRoom for first argument")
    end
    for i = 1, #boxes do
        if not boxes[i] or boxes[i]._type ~= "PTWalkBox" then
            error("PTRoomSetWalkBoxes: expected an array of PTWalkBox for second argument")
        end
        boxes[i].id = i
    end
    room.boxes = boxes
    room.box_links = PTGenLinksFromWalkBoxes(boxes)
    room.box_matrix = PTGenWalkBoxMatrix(#boxes, room.box_links)
end

--- Find the next walk box in the shortest path to reach a target walk box.
-- @tparam PTRoom room Room to query.
-- @tparam integer from_id ID of the starting @{PTWalkBox} in the room.
-- @tparam integer to_id ID of the target @{PTWalkBox} in the room.
-- @treturn PTWalkBox Next walk box in the shortest path, or nil if there is no path.
PTRoomGetNextBox = function(room, from_id, to_id)
    if not room or room._type ~= "PTRoom" then
        error("PTRoomGetNextBox: expected PTRoom for first argument")
    end
    if from_id <= 0 or from_id > #room.boxes then
        error("PTRoomGetNextBox: argument 2 out of range")
    end
    if to_id <= 0 or to_id > #room.boxes then
        error("PTRoomGetNextBox: argument 3 out of range")
    end
    if from_id == to_id then
        return room.boxes[from_id]
    end
    local target = room.box_matrix[from_id][to_id]
    if target == 0 then
        return nil
    end
    return room.boxes[target]
end

--- Convert coordinates in screen space to room space.
-- Uses the current room.
-- @tparam integer x X coordinate in screen space.
-- @tparam integer y Y coordinate in screen space.
-- @treturn integer X coordinate in room space.
-- @treturn integer Y coordinate in room space.
PTScreenToRoom = function(x, y)
    local room = PTCurrentRoom()
    if not room or room._type ~= "PTRoom" then
        return x, y
    end
    return (room.x - room.origin_x) + x, (room.y - room.origin_y) + y
end

--- Convert coordinates in room space to screen space.
-- Uses the current room.
-- @tparam integer x X coordinate in room space.
-- @tparam integer y Y coordinate in room space.
-- @tparam[opt=1] float parallax_x X parallax scaling factor.
-- @tparam[opt=1] float parallax_y Y parallax scaling factor.
-- @treturn X coordinate in screen space.
-- @treturn Y coordinate in screen space.
PTRoomToScreen = function(x, y, parallax_x, parallax_y)
    local room = PTCurrentRoom()
    if not room or room._type ~= "PTRoom" then
        return x, y
    end
    if not parallax_x then
        parallax_x = 1
    end
    if not parallax_y then
        parallax_y = 1
    end
    local room_x, room_y = room.x + (room.sx or 0), room.y + (room.sy or 0)
    local dx, dy = math.floor((x - room_x) * parallax_x), math.floor((y - room_y) * parallax_y)
    return dx + room.origin_x, dy + room.origin_y
end

--- Return the current room.
-- @treturn PTRoom The current PTRoom.
PTCurrentRoom = function()
    return _PTRoomList[_PTCurrentRoom]
end

--- Return a room by name.
-- @tparam string name Name of the room.
-- @treturn PTRoom The PTRoom, or nil.
PTGetRoom = function(name)
    if type(name) ~= "string" then
        error("PTGetRoom: expected string for first argument")
    end
    return _PTRoomList[name]
end

--- Add a room to the engine state.
-- @tparam PTRoom room The room to add.
PTAddRoom = function(room)
    if not room or room._type ~= "PTRoom" then
        error("PTAddRoom: expected PTRoom for first argument")
    end
    if type(room.name) ~= "string" then
        error("PTAddRoom: room object needs a name")
    end
    _PTRoomList[room.name] = room
end

--- Remove a room from the engine state.
-- @tparam PTRoom room The room to remove.
PTRemoveRoom = function(room)
    if not room or room._type ~= "PTRoom" then
        error("PTRemoveRoom: expected PTRoom for first argument")
    end
    if type(room.name) ~= "string" then
        error("PTRemoveRoom: room object needs a name")
    end

    _PTRoomList[room.name] = nil
end

local _PTRoomTalkUpdate = function(room, fast_forward)
    local result = true
    if room.talk_next_wait then
        if room.talk_next_wait < 0 then
            -- negative talk wait time means wait until click
            result = false
        elseif not fast_forward and _PTGetMillis() < room.talk_next_wait then
            result = false
        else
            if room.talk_img then
                PTRoomRemoveObject(room, room.talk_img)
                room.talk_img = nil
            end
            room.talk_next_wait = nil
        end
    end
    return result
end

local _PTUpdateRoom = function(force)
    if force == nil then
        force = false
    end

    local room = PTCurrentRoom()
    if not room or room._type ~= "PTRoom" then
        return
    end
    if not force then
        if _PTGamePaused then
            return
        end
    end
    for i, actor in ipairs(room.actors) do
        _PTActorUpdate(actor, false)
        --print(string.format("pos: (%d, %d), walkdata_cur: (%d, %d), walkdata_next: (%d, %d), walkdata_delta_factor: (%d, %d)", actor.x, actor.y, actor.walkdata_cur.x, actor.walkdata_cur.y, actor.walkdata_next.x, actor.walkdata_next.y, actor.walkdata_delta_factor.x, actor.walkdata_delta_factor.y))
    end
    -- Update room text
    _PTRoomTalkUpdate(room, false)

    -- constrain camera to room bounds
    local sw, sh = _PTGetScreenDims()
    local x_min = room.origin_x
    local x_max = room.width - (sw - room.origin_x)
    local y_min = room.origin_y
    local y_max = room.height - (sh - room.origin_y)
    room.x = math.max(math.min(room.x, x_max), x_min)
    room.y = math.max(math.min(room.y, y_max), y_min)
end

--- Switch the current room.
-- Will call the callbacks specified by @{PTOnRoomEnter} and
-- @{PTOnRoomExit}.
-- @tparam string name Room name to switch to. Must have been added to the engine state with @{PTAddRoom}.
-- @tparam table ctx Optional Context data to pass to the callbacks.
PTSwitchRoom = function(name, ctx)
    if type(name) ~= "string" then
        error("PTSwitchRoom: first argument must be a string")
    end
    local current = PTCurrentRoom()
    local room = PTGetRoom(name)
    if not room or room._type ~= "PTRoom" then
        error("PTSwitchRoom: no target room found with name %s", name)
    end
    if current and _PTOnRoomExitHandlers[current.name] then
        PTLog("PTSwitchRoom: calling exit handler for %s", current.name)
        _PTOnRoomExitHandlers[current.name](ctx)
    end
    if current and _PTOnRoomUnloadHandlers[current.name] then
        PTLog("PTSwitchRoom: calling unload handler for %s", current.name)
        _PTOnRoomUnloadHandlers[current.name](ctx)
    end
    _PTCurrentRoom = room.name
    if room and _PTOnRoomLoadHandlers[room.name] then
        PTLog("PTSwitchRoom: calling load handler for %s", room.name)
        _PTOnRoomLoadHandlers[room.name](ctx)
    end
    if room and _PTOnRoomEnterHandlers[room.name] then
        PTLog("PTSwitchRoom: calling enter handler for %s", room.name)
        _PTOnRoomEnterHandlers[room.name](ctx)
    end
    _PTUpdateRoom(true)
end

--- Sleep the current thread until a room finishes the action in progress.
-- @tparam PTRoom room The @{PTRoom} to wait for.
PTWaitForRoom = function(room)
    if type(room) ~= "table" or room._type ~= "PTRoom" then
        error(string.format("PTWaitForRoom(): argument must be a PTRoom"))
    end
    local thread, _ = coroutine.running()
    for k, v in pairs(_PTThreads) do
        if v == thread then
            _PTThreadsRoomWait[k] = room
            coroutine.yield()
            return
        end
    end
    error(string.format("PTWaitForRoom(): thread not found"))
end

local _PTRoomWaitAfterTalk = true
--- Set whether to automatically wait after a PTRoom starts talking.
-- If enabled, this means threads can make successive calls to
-- @{PTRoomTalk} and the engine will treat them as a conversation;
-- you don't need to explicitly call @{PTWaitForRoom} after each one.
-- If you want to do manual conversation timing, disable this feature.
-- Defaults to true.
-- @tparam boolean enable Whether to wait after talking.
PTSetRoomWaitAfterTalk = function(enable)
    _PTRoomWaitAfterTalk = enable
end

--- Make the current room talk.
-- This is useful for displaying disembodied voices that aren't attached to an actor.
-- By default, this will wait the thread until the room finishes talking. You can disable this by calling @{PTSetRoomWaitAfterTalk}.
-- @tparam integer x X position of message, in room coordinates.
-- @tparam integer y Y position of message, in room coordinates.
-- @tparam string message Message to show on the screen.
-- @tparam[opt=nil] PTFont font Font to use. Defaults to actor.talk_font
-- @tparam[opt=nil] table colour Inner colour; list of 3 8-bit numbers. Defaults to actor.talk_colour.
-- @tparam[opt=nil] integer duration Duration of the message, in milliseconds. By default this varies based on the length of the message; see @{PTGetTalkBaseDelay} and @{PTGetTalkCharDelay}.
PTRoomTalk = function(x, y, message, font, colour, duration)
    local room = PTCurrentRoom()
    if not room or room._type ~= "PTRoom" then
        error("PTRoomTalk: no current room set")
    end
    if not font then
        font = room.talk_font
    end
    if not font or font._type ~= "PTFont" then
        PTLog("PTRoomTalk: no font argument, or actor has invalid talk_font")
        return
    end

    if not colour then
        colour = room.talk_colour
    end
    if PTThreadInFastForward() then
        return
    end
    local text = PTText(message, font, 200, "center", colour)
    PTSetImageOriginSimple(text, "bottom")
    local width, height = PTGetImageDims(text)
    local sx, sy = PTRoomToScreen(x, y)
    local sw, sh = _PTGetScreenDims()

    sx = math.min(math.max(sx, width / 2), sw - width / 2)
    sy = math.min(math.max(sy, height), sh)
    x, y = PTScreenToRoom(sx, sy)

    if not room.talk_img then
        room.talk_img = PTBackground(nil, 0, 0, 20)
        PTRoomAddObject(room, room.talk_img)
    end
    room.talk_img.x = x
    room.talk_img.y = y
    room.talk_img.image = text
    if duration then
        room.talk_next_wait = _PTGetMillis() + duration
    elseif _PTTalkBaseDelay < 0 or _PTTalkCharDelay < 0 then
        room.talk_next_wait = -1
    else
        room.talk_next_wait = _PTGetMillis() + _PTTalkBaseDelay + #message * _PTTalkCharDelay
    end
    if _PTRoomWaitAfterTalk then
        PTWaitForRoom(room)
    end
end

--- Make the current room stop talking.
-- This will remove any speech bubble.
PTRoomSilence = function()
    local room = PTCurrentRoom()
    if not room or room._type ~= "PTRoom" then
        error("PTRoomSilence: no current room set")
    end
    if room.talk_img then
        PTRoomRemoveObject(room, room.talk_img)
        room.talk_img = nil
    end
    room.talk_next_wait = nil
end

--- Sleep the current thread.
-- This is mechanically the same as @{PTSleep}, however it uses the same
-- timer as @{PTRoomTalk}, meaning it can be skipped by clicking the mouse.
-- This is useful for e.g. dramatic pauses in dialogue.
-- @tparam integer millis Time to wait in milliseconds.
PTRoomSleep = function(millis)
    local room = PTCurrentRoom()
    if not room or room._type ~= "PTRoom" then
        error("PTRoomSleep: no current room set")
    end
    if PTThreadInFastForward() then
        return
    end
    room.talk_next_wait = _PTGetMillis() + millis
    if _PTRoomWaitAfterTalk then
        PTWaitForRoom(room)
    end
end

--- Verbs
-- @section verb

local _PTVerbCallbacks = {}
local _PTVerb2Callbacks = {}

--- Set a callback for a single-subject verb action.
-- @tparam string verb Verb to use.
-- @tparam string subject Subject of the verb.
-- @tparam function callback Function body to call, with the verb and subject as arguments.
PTOnVerb = function(verb, subject, callback)
    if not _PTVerbCallbacks[verb] then
        _PTVerbCallbacks[verb] = {}
    end
    _PTVerbCallbacks[verb][subject] = callback
end

local _PTCurrentVerb = nil
local _PTCurrentSubjectA = nil
local _PTCurrentSubjectB = nil
--- Run a single-subject verb action in the verb thread.
-- This will asynchronously run the callback set by @{PTOnVerb}.
-- Ideally your game's input code would call this - so e.g. on a
-- mouseDown event, if your game's UI had the "look" verb enabled,
-- you could fetch the current moused-over object ID and make a call
-- like PTDoVerb("look", "pinking shears").
-- @tparam string verb Verb to use.
-- @tparam string subject Subject of the verb.
PTDoVerb = function(verb, subject)
    PTLog("PTDoVerb: %s %s\n", tostring(verb), tostring(subject))
    _PTCurrentVerb = verb
    _PTCurrentSubjectA = subject
    _PTCurrentSubjectB = nil
end

local _PTVerbReadyCallback = nil
--- Set a callback for determining if the engine can trigger a verb action.
-- This can be useful for e.g. delaying a verb from being triggered
-- until an actor has finished walking to a location.
-- @tparam function callback Function body to call.
PTSetVerbReadyCheck = function(callback)
    _PTVerbReadyCallback = callback
end

--- Set a callback for a two-subject verb action.
-- @tparam string verb Verb to use.
-- @tparam string subject_a Subject A of the verb.
-- @tparam string subject_b Subject B of the verb.
-- @tparam function callback Function body to call, with the verb and the two subjects as arguments.
-- @tparam[opt=false] boolean directional Whether to invoke this callback only in a single direction; i.e. on @{PTDoVerb2}("verb", "a", "b") but not on @{PTDoVerb2}("verb", "b", "a").
PTOnVerb2 = function(verb, subject_a, subject_b, callback, directional)
    if directional == nil then
        directional = false
    end
    if not _PTVerb2Callbacks[verb] then
        _PTVerb2Callbacks[verb] = {}
    end
    -- forwards case
    if not _PTVerb2Callbacks[verb][subject_a] then
        _PTVerb2Callbacks[verb][subject_a] = {}
    end
    _PTVerb2Callbacks[verb][subject_a][subject_b] = callback
    -- reverse case
    if not directional then
        if not _PTVerb2Callbacks[verb][subject_b] then
            _PTVerb2Callbacks[verb][subject_b] = {}
        end
        _PTVerb2Callbacks[verb][subject_b][subject_a] = callback
    end
end

--- Run a two-subject verb action in the verb thread.
-- This will asynchronously run the callback set by @{PTOnVerb2}.
-- Ideally your game's input code would call this - so e.g. on a
-- mouseDown event, if your game's UI had the "give" verb enabled,
-- and an inventory item selected, you could fetch the current
-- moused-over object ID and make a call like PTDoVerb2("give", "pinking shears", "rock wallaby").
-- @tparam string verb Verb to use.
-- @tparam string subject_a First subject of the verb.
-- @tparam string subject_b Second subject of the verb.
PTDoVerb2 = function(verb, subject_a, subject_b)
    PTLog("PTDoVerb2: %s %s %s\n", tostring(verb), tostring(subject_a), tostring(subject_b))
    _PTCurrentVerb = verb
    _PTCurrentSubjectA = subject_a
    _PTCurrentSubjectB = subject_b
end

local _PTGrabInputOnVerb = true
--- Set whether to grab the user input when a verb callback is run.
-- If enabled, this means calling any verb action with @{PTDoVerb} or
-- @{PTDoVerb2} will automatically call @{PTGrabInput} at the start and @{PTReleaseInput}
-- at the end.
-- Defaults to true.
-- @tparam boolean enable Whether to grab user input.
PTSetGrabInputOnVerb = function(enable)
    _PTGrabInputOnVerb = enable
end

--- Return whether the current queued verb action is ready.
-- If there is a callback set by @{PTSetVerbReadyCheck}, this will return the response from that.
-- @treturn boolean Whether the queued verb action is ready.
PTVerbReady = function()
    if
        _PTCurrentVerb ~= nil
        and _PTCurrentSubjectA ~= nil
        and (
            (
                _PTCurrentSubjectB ~= nil
                and _PTVerb2Callbacks[_PTCurrentVerb]
                and _PTVerb2Callbacks[_PTCurrentVerb][_PTCurrentSubjectA]
                and _PTVerb2Callbacks[_PTCurrentVerb][_PTCurrentSubjectA][_PTCurrentSubjectB]
            )
            or (
                _PTCurrentSubjectB == nil
                and _PTVerbCallbacks[_PTCurrentVerb]
                and _PTVerbCallbacks[_PTCurrentVerb][_PTCurrentSubjectA]
            )
        )
    then
        if _PTVerbReadyCallback then
            return _PTVerbReadyCallback()
        end
        return true
    end
    return false
end

--- Process any outstanding verb action.
-- @local
local _PTRunVerb = function()
    if PTVerbReady() then
        if PTThreadExists("__verb") then
            -- don't allow for interrupting another verb
            PTLog(
                "PTRunVerb(): attempted to replace running verb thread with %s %s %s!",
                _PTCurrentVerb,
                tostring(_PTCurrentSubjectA),
                tostring(_PTCurrentSubjectB)
            )
        else
            if _PTCurrentSubjectB ~= nil then
                PTStartThread(
                    "__verb",
                    _PTVerb2Callbacks[_PTCurrentVerb][_PTCurrentSubjectA][_PTCurrentSubjectB],
                    _PTCurrentVerb,
                    _PTCurrentSubjectA,
                    _PTCurrentSubjectB
                )
            else
                PTStartThread(
                    "__verb",
                    _PTVerbCallbacks[_PTCurrentVerb][_PTCurrentSubjectA],
                    _PTCurrentVerb,
                    _PTCurrentSubjectA
                )
            end
            if _PTGrabInputOnVerb then
                PTGrabInput()
            end
        end
        _PTCurrentVerb = nil
        _PTCurrentSubjectA = nil
        _PTCurrentSubjectB = nil
    end
end

local _PTGamePaused = false
PTPauseGame = function()
    _PTGamePaused = true
end

PTUnpauseGame = function()
    _PTGamePaused = false
end

PTGetGamePaused = function()
    return _PTGamePaused
end

--- Error handler. Called from C.
-- @local
_PTWhoops = function(err, thread)
    local preamble = string.format(
        "Unhandled script error!\nPlease send this info to the game author so they can fix the problem.\n\nPerentie version: %s, Game: %s (%s) ver. %s\nError: %s",
        _PTVersion(),
        tostring(_PTGameID),
        tostring(_PTGameName),
        tostring(_PTGameVersion),
        tostring(err)
    )
    if thread then
        return debug.traceback(thread, preamble)
    else
        return debug.traceback(preamble)
    end
end

--- Run and handle execution for all of the threads.
-- @local
-- @treturn integer Number of threads still alive.
_PTRunThreads = function()
    -- If the game is paused, don't run coroutines.
    if not _PTGamePaused then
        _PTRunVerb()
    end
    local count = 0
    for name, thread in pairs(_PTThreads) do
        if not _PTGamePaused or (name == "__gui") then
            -- Check if the thread is supposed to be asleep
            local is_awake = true
            if _PTThreadsActorWait[name] then
                is_awake = _PTActorUpdate(_PTThreadsActorWait[name], _PTThreadsFastForward[name])
                if is_awake then
                    _PTThreadsActorWait[name] = nil
                end
            elseif _PTThreadsRoomWait[name] then
                is_awake = _PTRoomTalkUpdate(_PTThreadsRoomWait[name], _PTThreadsFastForward[name])
                if is_awake then
                    _PTThreadsRoomWait[name] = nil
                end
            elseif _PTThreadsMoveObjectWait[name] then
                is_awake = not _PTObjectIsMoving(_PTThreadsMoveObjectWait[name], _PTThreadsFastForward[name])
                --print("MoveObjectWait", is_awake)
                if is_awake then
                    _PTThreadsMoveObjectWait[name] = nil
                end
            elseif _PTThreadsAnimationWait[name] then
                is_awake = not _PTThreadsAnimationWait[name].looping
                    and _PTThreadsAnimationWait[name].current_frame == #_PTThreadsAnimationWait[name].frames
                if is_awake then
                    _PTThreadsAnimationWait[name] = nil
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
                if _PTWatchdogEnabled then
                    debug.sethook(thread, _PTWatchdog, "", _PTWatchdogLimit)
                end
                -- Resume the thread, let it run until the next sleep command
                local success, result = coroutine.resume(thread)
                -- Pet the watchdog for a job well done
                debug.sethook(thread)

                -- Handle the response from the execution run.
                if not success then
                    PTLogError(_PTWhoops(result, _PTThreads[name]))
                end
                local status = coroutine.status(thread)
                if status == "dead" then
                    --PTLog("PTRunThreads(): Thread %s terminated", name)
                    coroutine.close(_PTThreads[name])
                    _PTThreads[name] = nil
                    _PTThreadsSleepUntil[name] = nil
                    _PTThreadsActorWait[name] = nil
                    _PTThreadsFastForward[name] = nil
                    if name == "__verb" and _PTGrabInputOnVerb then
                        PTReleaseInput()
                    end
                else
                    count = count + 1
                end
            else
                count = count + 1
            end
        end
    end
    return count
end

local _PTMouseSprite = nil
PTSetMouseSprite = function(sprite)
    _PTMouseSprite = sprite
end

--- Debug
-- @section debug

local _PTWalkBoxDebug = false
--- Set whether to draw the walkbox of the current room
-- @tparam boolean val true if the walk box is to be drawn, false otherwise.
PTSetWalkBoxDebug = function(val)
    _PTWalkBoxDebug = val
end

local _PTImageDebug = false
--- Set whether to draw the bounding boxes of images.
-- @tparam boolean val true if the image bounding boxes are to be drawn, false otherwise.
PTSetImageDebug = function(val)
    _PTImageDebug = val
end

--- Set whether to enable the debug console.
-- @tparam boolean enable true if the debug console is to be enabled, false otherwise.
-- @tparam[opt=""] string device The device to attach the console to; e.g. "COM4"
PTSetDebugConsole = function(enable, device)
    if device == nil then
        device = ""
    end
    _PTSetDebugConsole(enable, device)
end

--- Process the main event loop. Called from C.
-- @local
_PTEvents = function()
    local ev = _PTPumpEvent()
    while ev do
        if _PTInputGrabbed then
            -- Grabbed input mode, intercept events
            if ev.type == "keyDown" and ev.key == "escape" then
                PTFastForwardThread("__verb", true)
                if _PTFastForwardWhileGrabbed then
                    _PTFastForwardWhileGrabbed()
                end
            elseif (ev.type == "mouseDown") or (ev.type == "keyDown" and ev.key == ".") then
                PTTalkSkipThread("__verb", true)
                if _PTTalkSkipWhileGrabbed then
                    _PTTalkSkipWhileGrabbed()
                end
            end
        elseif not _PTGamePaused then
            -- Don't run event consumers while paused
            if _PTEventConsumers[ev.type] then
                _PTEventConsumers[ev.type](ev)
            end
        end
        _PTGUIEvent(ev)
        ev = _PTPumpEvent()
    end
    if not _PTInputGrabbed and not _PTGamePaused then
        _PTUpdateMouseOver()
    end
    _PTUpdateRoom()
    _PTUpdateMoveObject()
    _PTUpdateShakeObject()
    _PTUpdateGUI()

    if _PTSaveIndex then
        local index = _PTSaveIndex
        local state_name = _PTSaveStateName
        _PTSaveIndex = nil
        _PTSaveStateName = nil
        _PTSaveToStateFile(index, state_name)
    end
end

--- Process the main rendering loop. Called from C.
-- @local
_PTRender = function()
    local room = PTCurrentRoom()
    if not room or room._type ~= "PTRoom" then
        return
    end
    if _PTAutoClearScreen then
        _PTClearScreen()
    end
    if _PTRenderFrameConsumer then
        _PTRenderFrameConsumer()
    end
    local debugBuffer = {}

    local blit = function(img, x, y, flags)
        x, y = math.floor(x), math.floor(y)
        if img then
            PTDrawImage(img, x, y, flags)
            if _PTImageDebug then
                local w, h = PTGetImageDims(img)
                local ox, oy = PTGetImageOrigin(img)
                table.insert(debugBuffer, { x - ox, y - oy, x - ox + w - 1, y - oy + h - 1, x, y })
            end
        end
    end

    for obj, x, y in PTIterObjects(room.render_list) do
        local frame, flags = PTGetImageFromObject(obj)
        if frame then
            local tmp_x, tmp_y = PTRoomToScreen(x, y, obj.parallax_x, obj.parallax_y)
            blit(frame, tmp_x, tmp_y, flags)
        end
    end
    if _PTWalkBoxDebug then
        for i, box in pairs(room.boxes) do
            local ul_x, ul_y = PTRoomToScreen(box.ul.x, box.ul.y)
            local ur_x, ur_y = PTRoomToScreen(box.ur.x, box.ur.y)
            local lr_x, lr_y = PTRoomToScreen(box.lr.x, box.lr.y)
            local ll_x, ll_y = PTRoomToScreen(box.ll.x, box.ll.y)
            _PTDrawLine(ul_x, ul_y, ur_x, ur_y, 0xff, 0x55, 0x55)
            _PTDrawLine(ur_x, ur_y, lr_x, lr_y, 0xff, 0x55, 0x55)
            _PTDrawLine(lr_x, lr_y, ll_x, ll_y, 0xff, 0x55, 0x55)
            _PTDrawLine(ll_x, ll_y, ul_x, ul_y, 0xff, 0x55, 0x55)
        end
    end
    for obj, x, y in PTIterObjects(_PTGlobalRenderList) do
        local frame, flags = PTGetImageFromObject(obj)
        if frame then
            blit(frame, x, y, flags)
        end
    end
    for _, panel in ipairs(_PTPanelList) do
        if panel.visible then
            for obj, x, y in PTIterObjects({ panel }) do
                local frame, flags = PTGetImageFromObject(obj)
                if frame then
                    blit(frame, x, y, flags)
                end
            end
        end
    end

    if _PTImageDebug then
        for _, t in pairs(debugBuffer) do
            _PTDrawLine(t[1], t[2], t[3], t[2], 0x55, 0x55, 0xff)
            _PTDrawLine(t[3], t[2], t[3], t[4], 0x55, 0x55, 0xff)
            _PTDrawLine(t[3], t[4], t[1], t[4], 0x55, 0x55, 0xff)
            _PTDrawLine(t[1], t[4], t[1], t[2], 0x55, 0x55, 0xff)
        end
        for _, t in pairs(debugBuffer) do
            _PTDrawLine(t[5], t[6], t[5], t[6], 0x55, 0xff, 0xff)
        end
    end

    if _PTMouseSprite and not _PTInputGrabbed then
        local mouse_x, mouse_y = _PTGetMousePos()
        for obj, x, y in PTIterObjects({ _PTMouseSprite }) do
            local frame, flags = PTGetImageFromObject(obj)
            if frame then
                _PTDrawImage(frame.ptr, mouse_x, mouse_y, flags)
            end
        end
    end
end
