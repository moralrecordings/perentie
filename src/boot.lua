--- The Perentie scripting API.
-- @module perentie

--- General
-- @section general

SCREEN_WIDTH = 320
SCREEN_HEIGHT = 200

--- Get the version string of the Perentie engine.
-- @treturn string Version string.
PTVersion = function()
    return _PTVersion()
end

--- Print a message to the Perentie log file. This is a more accessible replacement for Lua's @{print} function, which will only output to the debug console.
-- @tparam string format Format string in @{string.format} syntax.
-- @param ... Arguments for format string.
PTLog = function(...)
    return _PTLog(string.format(...))
end

--- Get the number of milliseconds elapsed since the engine started.
-- @treturn integer Number of milliseconds.
PTGetMillis = function()
    return _PTGetMillis()
end

--- Quit Perentie.
-- @tparam[opt=0] int retcode Return code.
PTQuit = function(retcode)
    if not retcode then
        retcode = 0
    end
    _PTQuit(retcode)
end

_PTAddToList = function(list, object)
    local exists = false
    for i, obj in ipairs(list) do
        if object == obj then
            exists = true
            break
        end
    end
    if not exists then
        table.insert(list, object)
    end
end

_PTRemoveFromList = function(list, object)
    for i, obj in ipairs(list) do
        if object == obj then
            table.remove(list, i)
            break
        end
    end
end

--- Input
-- @section input

--- Get the last recorded mouse position.
-- @treturn integer X coordinate in screen space.
-- @treturn integer Y coordinate in screen space.
PTGetMousePos = function()
    return _PTGetMousePos()
end

--- Actors
-- @section actor

--- Actor structure.
-- @tfield string _type "PTActor"
-- @tfield[opt=0] integer x X coordinate in room space.
-- @tfield[opt=0] integer y Y coordinate in room space.
-- @tfield[opt=0] integer z Depth coordinate; a higher number renders to the front.
-- @tfield[opt=true] boolean visible
-- @tfield[opt=nil] PTRoom room The room the actor is located.
-- @tfield[opt=nil] table sprite The @{PTBackground}/@{PTSprite} object used for drawing. Perentie will proxy this; you only need to add the actor object to the rendering list.
-- @tfield[opt=0] integer talk_x X coordinate in actor space for talk text.
-- @tfield[opt=0] integer talk_y Y coordinate in actor space for talk text.
-- @tfield[opt=nil] PTImage talk_img Handle used by the engine for caching the rendered talk text.
-- @tfield[opt=nil] PTFont talk_font Font to use for rendering talk text.
-- @tfield[opt={ 0xff 0xff 0xff }] table talk_colour Colour to use for rendering talk text.
-- @tfield[opt=0] integer talk_next_wait The millisecond count at which to remove the talk text.
-- @tfield[opt=0] integer facing Direction of the actor; angle in degrees clockwise from north.
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
        walkbox = nil,
        facing = 0,
        scale_x = 0xff,
        scale_y = 0xff,
        speed_x = 8,
        speed_y = 4,
        walk_rate = 12,
        walkdata_next_wait = 0,
        moving = 0,
    }
end

PTActorSetWalkBox = function(actor, box)
    if not actor or actor._type ~= "PTActor" then
        error("PTActorSetWalkBox: expected PTActor for first argument")
    end
    if not box or box._type ~= "PTWalkBox" then
        error("PTActorSetWalkBox: expected PTWalkBox for second argument")
    end
    actor.walkbox = box
    if box.z and box.z ~= actor.z then
        actor.z = box.z
        -- update the render list order
        PTRoomAddObject(actor.room, nil)
    end
end

PTActorUpdate = function(actor, fast_forward)
    if not actor or actor._type ~= "PTActor" then
        error("PTActorUpdate: expected PTActor for first argument")
    end
    if actor.talk_img and actor.talk_next_wait then
        if not fast_forward and _PTGetMillis() < actor.talk_next_wait then
            return false
        else
            PTRoomRemoveObject(actor.room, actor.talk_img)
            actor.talk_img = nil
        end
    end
    return true
end

TALK_BASE_DELAY = 1000
TALK_CHAR_DELAY = 85

PTActorTalk = function(actor, message)
    if not actor or actor._type ~= "PTActor" then
        error("PTActorTalk: expected PTActor for first argument")
    end
    if not actor.talk_font then
        PTLog("PTActorTalk: no default font!!")
        return
    end
    local text = PTText(message, actor.talk_font, 200, "center", actor.talk_colour)

    local width, height = PTGetImageDims(text)
    PTSetImageOrigin(text, width / 2, height)
    local x = math.min(math.max(actor.x + actor.talk_x, width / 2), SCREEN_WIDTH - width / 2)
    local y = math.min(math.max(actor.y + actor.talk_y, height), SCREEN_HEIGHT)

    actor.talk_img = PTBackground(text, x, y, 10)
    actor.talk_next_wait = _PTGetMillis() + TALK_BASE_DELAY + #message * TALK_CHAR_DELAY
    -- TODO: Make room reference on actor?
    PTRoomAddObject(PTCurrentRoom(), actor.talk_img)
end

--- Rendering
-- @section rendering

--- Image structure.
-- @tfield string _type "PTImage"
-- @tfield userdata ptr Pointer to C data.
-- @table PTImage

--- Create a new image.
-- @tparam string path Path of the image (must be 8-bit indexed or grayscale PNG).
-- @tparam[opt=0] integer origin_x Origin x coordinate, relative to top-left corner.
-- @tparam[opt=0] integer origin_y Origin y coordinate, relative to top-left corner.
-- @tparam[opt=-1] integer colourkey Palette index to use as colourkey.
-- @treturn PTImage The new image.
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

FLIP_H = 0x01
FLIP_V = 0x02

--- Get the dimensions of an image
-- @tparam PTImage image Image to query.
-- @treturn integer Width of the image.
-- @treturn integer Height of the image.
PTGetImageDims = function(image)
    if not image or image._type ~= "PTImage" then
        return 0, 0
    end
    return _PTGetImageDims(image.ptr)
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
    return _PTSetImageOrigin(image.ptr, x, y)
end

--- Calculate the delta angle between two directions.
-- @tparam integer src Start direction, in degrees clockwise from north.
-- @tparam integer dest End direction, in degrees clockwise from north.
-- @tresult integer Angle between the two directions, in degrees clockwise.
PTAngleDelta = function(src, dest)
    return ((dest - src + 180) % 360) - 180
end

--- Calculate a direction angle mirrored by a plane.
-- @tparam integer src Start direction, in degrees clockwise from north.
-- @tparam integer plane Plane direction, in degrees clockwise from north.
-- @tresult integer Reflected direction, in degrees clockwise from north.
PTAngleMirror = function(src, plane)
    local delta = PTAngleDelta(plane, src)
    return (plane - delta + 360) % 360
end

--- Background structure.
-- @tfield string _type "PTBackground"
-- @tfield[opt=0] integer x X coordinate in room space.
-- @tfield[opt=0] integer y Y coordinate in room space.
-- @tfield[opt=0] integer z Depth coordinate; a higher number renders to the front.
-- @tfield[opt=false] boolean collision Whether to test this object's sprite mask for collisions; e.g. when updating the current @{PTGetMouseOver} object.
-- @tfield[opt=true] boolean visible Whether to draw this object to the screen.
-- @table PTBackground

--- Create a new background.
-- @tparam PTImage image Image to use.
-- @tparam[opt=0] integer x X coordinate in room space.
-- @tparam[opt=0] integer y Y coordinate in room space.
-- @tparam[opt=0] integer z Depth coordinate; a higher number renders to the front.
-- @treturn PTBackground The new background.
PTBackground = function(image, x, y, z)
    if not x then
        x = 0
    end
    if not y then
        y = 0
    end
    if not z then
        z = 0
    end
    return { _type = "PTBackground", image = image, x = x, y = y, z = z, collision = false, visible = true }
end

--- Animation structure.
-- @tfield string _type "PTAnimation"
-- @tfield string name Name of the animation.
-- @tfield table frames List of @{PTImage} objects; one per frame.
-- @tfield[opt=0] integer rate Frame rate to use for playback.
-- @tfield[opt=0] integer facing Direction of the animation; angle in degrees clockwise from north.
-- @tfield[opt=false] boolean looping Whether to loop the animation when completed.
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
-- @treturn PTAnimation The new animation.
PTAnimation = function(name, frames, rate, facing, h_mirror, v_mirror)
    if not rate then
        rate = 0
    end
    if not facing then
        facing = 0
    end
    if not h_mirror then
        h_mirror = false
    end
    if not v_mirror then
        v_mirror = false
    end
    return {
        _type = "PTAnimation",
        name = name,
        frames = frames,
        rate = rate,
        facing = facing,
        looping = false,
        current_frame = 0,
        next_wait = 0,
        h_mirror = h_mirror,
        v_mirror = v_mirror,
        flags = 0,
    }
end

--- Sprite structure.
-- @tfield string _type "PTSprite"
-- @tfield table animations Table of @{PTAnimation} objects indexed by name.
-- @tfield[opt=0] integer x X coordinate in room space.
-- @tfield[opt=0] integer y Y coordinate in room space.
-- @tfield[opt=0] integer z Depth coordinate; a higher number renders to the front.
-- @tfield[opt=nil] integer anim_index Index of the current animation from the animations table.
-- @tfield[opt=0] integer anim_flags Transformation flags to be applied to the frames.
-- @tfield[opt=false] boolean collision Whether to test this object's sprite mask for collisions; e.g. when updating the current @{PTGetMouseOver} object.
-- @tfield[opt=true] boolean visible Whether to draw this object to the screen.
-- @table PTSprite

--- Create a new sprite.
-- @tparam table animations Table of @{PTAnimation} objects indexed by name.
-- @tparam[opt=0] integer x X coordinate in room space.
-- @tparam[opt=0] integer y Y coordinate in room space.
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
            sprite.anim_index = best_index
            sprite.anim_flags = best_flags
            return true
        end
    end
    return false
end

PTSpriteIncrementFrame = function(object)
    if object and object._type == "PTSprite" then
        local anim = object.animations[object.anim_index]
        if anim then
            if anim.current_frame == 0 then
                anim.current_frame = 1
            else
                anim.current_frame = (anim.current_frame % #anim.frames) + 1
            end
            print(string.format("PTSpriteIncrementFrame: %d", anim.current_frame))
        end
    end
end

--- Fetch the image to use when rendering a @{PTActor}/@{PTBackground}/@{PTSprite} object.
-- @tparam table object The object to query.
-- @treturn PTImage The image for the current frame.
-- @treturn integer Flags to render the image with.
PTGetAnimationFrame = function(object)
    if object and object._type == "PTSprite" then
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
                anim.current_frame = (anim.current_frame % #anim.frames) + 1
                anim.next_wait = PTGetMillis() + (1000 // anim.rate)
            end
            return anim.frames[anim.current_frame], object.anim_flags
        end
    elseif object and object._type == "PTBackground" then
        return object.image, 0
    elseif object and object._type == "PTActor" then
        return PTGetAnimationFrame(object.sprite)
    end
    return nil, 0
end

_PTGlobalRenderList = {}
--- Add a renderable (@{PTActor}/@{PTBackground}/@{PTSprite}) object to the global rendering list.
-- @tparam table object Object to add.
PTGlobalAddObject = function(object)
    if object then
        _PTAddToList(_PTGlobalRenderList, object)
    end
    table.sort(_PTGlobalRenderList, function(a, b)
        return a.z < b.z
    end)
end

--- Remove a renderable (@{PTActor}/@{PTBackground}/@{PTSprite}) object from the global rendering list.
-- @tparam table object Object to remove.
PTGlobalRemoveObject = function(object)
    if object then
        _PTRemoveFromList(_PTGlobalRenderList, object)
    end
    table.sort(_PTGlobalRenderList, function(a, b)
        return a.z < b.z
    end)
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
-- @treturn PTImage The new image.
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
-- @tfield integer id Internal ID of the walk box. Used internally by the box matrix.
-- @table PTWalkBox

--- Create a new walk box.
-- @tparam PTPoint ul Coordinates of upper-left corner.
-- @tparam PTPoint ur Coordinates of upper-right corner.
-- @tparam PTPoint lr Coordinates of lower-right corner.
-- @tparam PTPoint ll Coordinates of lower-left corner.
-- @tparam integer z Depth coordinate; a higher number renders to the front. Used for setting the depth of PTActors.
-- @treturn PTWalkBox The new walk box.
PTWalkBox = function(ul, ur, lr, ll, z)
    return { _type = "PTWalkBox", ul = ul, ur = ur, lr = lr, ll = ll, z = z, id = nil }
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
    for i, box in ipairs(boxes) do
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
    --print(string.format("PTAdjustPointToBeInBox(): point: (%d, %d), best_point: (%d, %d), best_box: %d", point.x, point.y, best_point.x, best_point.y, best_box.id))
    return best_point, best_box
end

local _PTFindPathTowards = function(start_x, start_y, dest_x, dest_y, b1, b2, b3)
    local box1 = PTWalkBox(b1.ul, b1.ur, b1.lr, b1.ll)
    local box2 = PTWalkBox(b2.ul, b2.ur, b2.lr, b2.ll)
    local found_path = PTPoint(0, 0)
    for i = 1, 4 do
        for j = 1, 4 do
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
    if actor.walkbox.id ~= actor.walkdata_curbox.id and _PTCheckPointInBoxBounds(actor, actor.walkdata_curbox) then
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
    PTSpriteSetAnimation(actor.sprite, "walk", actor.facing)
    return _PTActorWalkStep(actor)
end

--- Set an actor moving towards a point in the room.
-- @tparam PTActor actor Actor to modify.
-- @tparam integer x X coordinate in room space.
-- @tparam integer y Y coordinate in room space.
PTActorSetWalk = function(actor, x, y)
    if not actor or actor._type ~= "PTActor" then
        error("PTActorSetWalk: expected PTActor for first argument")
    end

    if not actor.room or actor.room._type ~= "PTRoom" then
        error("PTActorSetWalk: PTActor isn't assigned to a room")
    end

    local dest = PTPoint(x, y)
    local dest_point, dest_box = _PTAdjustPointToBeInBox(dest, actor.room.boxes)

    actor.walkdata_dest = dest_point
    actor.walkdata_destbox = dest_box
    actor.walkdata_curbox = actor.walkbox
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
        PTSpriteSetAnimation(actor.sprite, "stand", actor.facing)
        return
    end

    if actor.moving ~= MF_NEW_LEG then
        if _PTActorWalkStep(actor) then
            return
        end
        if actor.moving == MF_LAST_LEG then
            actor.moving = 0
            PTSpriteSetAnimation(actor.sprite, "stand", actor.facing)
            PTActorSetWalkBox(actor, actor.walkdata_destbox)
            -- turn anim here
        end

        if actor.moving == MF_TURN then
            -- update_actor_direction
        end

        PTActorSetWalkBox(actor, actor.walkdata_curbox)
        actor.moving = MF_IN_LEG
    end
    actor.moving = MF_NEW_LEG

    while true do
        if not actor.walkbox then
            PTActorSetWalkBox(actor, actor.walkdata_destbox)
            actor.walkdata_curbox = actor.walkdata_destbox
            break
        end

        if actor.walkbox.id == actor.walkdata_destbox.id then
            break
        end

        local next_box = PTRoomGetNextBox(actor.room, actor.walkbox.id, actor.walkdata_destbox.id)
        if not next_box then
            actor.moving = 0
            PTSpriteSetAnimation(actor.sprite, "stand", actor.facing)
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

        local result, found_path = _PTFindPathTowards(
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
        if result then
            break
        end
        if _PTCalcMovementFactor(actor, found_path) then
            return
        end

        PTActorSetWalkBox(actor, actor.walkdata_curbox)
    end

    actor.moving = MF_LAST_LEG
    _PTCalcMovementFactor(actor, actor.walkdata_dest)
end

PTActorSetRoom = function(actor, room, x, y)
    if not actor or actor._type ~= "PTActor" then
        error("PTActorSetRoom: expected PTActor for first argument")
    end
    if room and room._type ~= "PTRoom" then
        error("PTActorSetRoom: expected nil or PTRoom for second argument")
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
    if actor.room.boxes then
        local near_point, near_box = _PTAdjustPointToBeInBox(PTPoint(x, y), actor.room.boxes)
        actor.x, actor.y = near_point.x, near_point.y
        PTActorSetWalkBox(actor, near_box)
    else
        actor.x, actor.y = x, y
    end
end

--- Audio
-- @section audio

--- Play a square wave tone on the PC speaker.
-- The note will play until it is stopped by a call to PTStopBeep.
-- @tparam number freq Audio frequency of the tone.
PTPlayBeep = function(freq)
    return _PTPlayBeep(freq)
end

--- Stop playing a tone on the PC speaker.
PTStopBeep = function()
    return _PTStopBeep()
end

--- Load a music file in Reality Adlib Tracker format
-- @tparam string path The path to the file.
-- @tresult boolean Whether the file was successfully loaded.
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

--- Threading
-- @section threading

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

local _PTThreads = {}
local _PTThreadsSleepUntil = {}
local _PTThreadsActorWait = {}
local _PTThreadsFastForward = {}

--- Start a function in a new thread.
-- Perentie runs threads with cooperative multitasking; that is,
-- a long-running thread must use a sleep function like PTSleep
-- to yield control back to the engine.
-- Perentie will exit when all threads have stopped.
-- @tparam string name Name of the thread. Must be unique.
-- @tparam function func Function to run.
-- @param ... Arguments to pass to the function.
PTStartThread = function(name, func, ...)
    if _PTThreads[name] then
        error(string.format("PTStartThread(): thread named %s exists", name))
    end
    local varargs = ...
    if varargs then
        _PTThreads[name] = coroutine.create(function()
            func(table.unpack(varargs))
        end)
    else
        _PTThreads[name] = coroutine.create(function()
            func()
        end)
    end
end

--- Stop a running thread.
-- @tparam string name Name of the thread.
PTStopThread = function(name)
    if not _PTThreads[name] then
        error(string.format("PTStopThread(): thread named %s doesn't exist", name))
    end
    coroutine.close(_PTThreads[name])
    _PTThreads[name] = nil
    _PTThreadsFastForward[name] = nil
end

--- Check whether a thread is running.
-- @tparam string name Name of the thread.
-- @treturn bool Whether the thread exists.
PTThreadExists = function(name)
    return _PTThreads[name] ~= nil
end

--- Sleep the current thread.
-- Perentie uses co-operative multitasking; for long-running background
-- tasks it is important to call a wait or sleep function whenever possible,
-- even with a delay of 0.
-- Not doing so will cause the engine to freeze up, until the thread is
-- aborted early by the watchdog.
-- @tparam integer millis Time to wait in milliseconds.
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

--- Sleep the current thread until an actor finishes the action in progress.
-- @tparam PTActor actor The PTActor to wait for.
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

local _PTWatchdogEnabled = true
--- Toggle the use of the watchdog to abort threads that take too long.
-- Enabled by default.
-- @tparam bool enable Whether to enable the watchdog.
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
-- @tfield string name Name of the table.
-- @tfield integer width Width of the room in pixels.
-- @tfield integer height Height of the room in pixels.
-- @tfield integer x X coordinate of camera in room space.
-- @tfield integer y Y coordinate of camera in room space.
-- @tfield integer origin_x X coordinate of the camera offset in screen space.
-- @tfield integer origin_y Y coordinate of the camera offset in screen space.
-- @tfield table render_list List of renderable (@{PTActor}/@{PTBackground}/@{PTSprite}) objects in the room.
-- @tfield table boxes List of @{PTWalkBox} objects which make up the room's walkable area.
-- @tfield table box_links List of box ID pairs, each describing two directly connected walk boxes.
-- @tfield table box_matrix N x N matrix describing the shortest route between walk boxes; e.g. when starting from box ID i and trying to reach box ID j, box_matrix[i][j] is the ID of the next box you need to travel through in order to take the shortest path, or 0 if there is no path.

--- Create a new room.
-- @tparam string name Name of the room.
-- @tparam integer width Width of the room in pixels.
-- @tparam integer height Height of the room in pixels.
-- @treturn PTRoom The new room.
PTRoom = function(name, width, height)
    return {
        _type = "PTRoom",
        name = name,
        width = width,
        height = height,
        x = 0,
        y = 0,
        origin_x = SCREEN_WIDTH // 2,
        origin_y = SCREEN_HEIGHT // 2,
        render_list = {},
        boxes = {},
        box_links = {},
        box_matrix = { {} },
        actors = {},
        camera_actor = nil,
    }
end

--- Add a renderable (@{PTActor}/@{PTBackground}/@{PTSprite}) object to the room rendering list.
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

--- Remove a renderable (@{PTActor}/@{PTBackground}/@{PTSprite}) object from the room rendering list.
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

local _PTCurrentRoom = nil
local _PTOnRoomEnterHandlers = {}
local _PTOnRoomExitHandlers = {}

--- Convert coordinates in screen space to room space.
-- Uses the current room.
-- @tparam integer x X coordinate in screen space.
-- @tparam integer y Y coordinate in screen space.
-- @treturn int X coordinate in room space.
-- @treturn int Y coordinate in room space.
PTScreenToRoom = function(x, y)
    if not _PTCurrentRoom or _PTCurrentRoom._type ~= "PTRoom" then
        return x, y
    end
    return (_PTCurrentRoom.x - _PTCurrentRoom.origin_x) + x, (_PTCurrentRoom.y - _PTCurrentRoom.origin_y) + y
end

--- Convert coordinates in room space to screen space.
-- Uses the current room.
-- @tparam integer x X coordinate in room space.
-- @tparam integer y Y coordinate in room space.
-- @treturn X coordinate in screen space.
-- @treturn Y coordinate in screen space.
PTRoomToScreen = function(x, y)
    if not _PTCurrentRoom or _PTCurrentRoom._type ~= "PTRoom" then
        return x, y
    end
    return x - (_PTCurrentRoom.x - _PTCurrentRoom.origin_x), y - (_PTCurrentRoom.y - _PTCurrentRoom.origin_y)
end

--- Set a callback for switching to a particular room.
-- @tparam string name Name of the room.
-- @tparam function func Function body to call, with an optional argument
-- for context data.
PTOnRoomEnter = function(name, func)
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
    if _PTOnRoomExitHandlers[name] then
        PTLog("PTOnRoomExit: overwriting handler for %s", name)
    end
    _PTOnRoomExitHandlers[name] = func
end

--- Return the current room
-- @treturn PTRoom The current PTRoom.
PTCurrentRoom = function()
    return _PTCurrentRoom
end

--- Switch the current room.
-- Will call the callbacks specified by @{PTOnRoomEnter} and
-- @{PTOnRoomExit}.
-- @tparam PTRoom room Room object to switch to.
-- @tparam table ctx Optional Context data to pass to the callbacks.
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

--- Verbs
-- @section verb

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

local _PTVerbReadyCallback = nil
PTSetVerbReadyCheck = function(callback)
    _PTVerbReadyCallback = callback
end

--- Return whether the current queued verb action is ready.
-- @treturn bool Whether the
PTVerbReady = function()
    if
        _PTCurrentVerb ~= nil
        and _PTCurrentHotspot ~= nil
        and _PTVerbCallbacks[_PTCurrentVerb]
        and _PTVerbCallbacks[_PTCurrentVerb][_PTCurrentHotspot]
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
_PTRunVerb = function()
    if PTVerbReady() then
        if PTThreadExists("__verb") then
            -- interrupting another verb, stop the existing one
            PTLog(
                "PTRunVerb(): attempted to replace running verb thread with %s %s!",
                _PTCurrentVerb,
                _PTCurrentHotspot
            )
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

--- Run and handle execution for all of the threads.
-- @local
-- @treturn integer Number of threads still alive.
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
            if _PTWatchdogEnabled then
                debug.sethook(thread, _PTWatchdog, "", _PTWatchdogLimit)
            end
            -- Resume the thread, let it run until the next sleep command
            local success, result = coroutine.resume(thread)
            -- Pet the watchdog for a job well done
            debug.sethook(thread)

            -- Handle the response from the execution run.
            if not success then
                PTLog("PTRunThreads(): Thread %s errored:\n  %s", name, result)
                PTLog(debug.traceback(_PTThreads[name]))
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

local _PTWalkBoxDebug = false
--- Set whether to draw the walkbox of the current room
-- @tparam bool val true if the walk box is to be drawn, false otherwise.
PTSetWalkBoxDebug = function(val)
    _PTWalkBoxDebug = val
end

local _PTMouseOver = nil
--- Get the current object which the mouse is hovering over
-- @treturn table The object (@{PTActor}/@{PTBackground}/@{PTSprite}), or nil.
PTGetMouseOver = function()
    return _PTMouseOver
end

--- Set a callback for when the mouse moves over a new @{PTActor}/@{PTBackground}/@{PTSprite}.
-- @tparam function callback Function body to call, with the moused-over object as an argument.
local _PTMouseOverConsumer = nil
PTOnMouseOver = function(callback)
    _PTMouseOverConsumer = callback
end

local _PTUpdateMouseOver = function()
    local mouse_x, mouse_y = PTGetMousePos()
    local room_x, room_y = PTScreenToRoom(mouse_x, mouse_y)
    if not _PTCurrentRoom or _PTCurrentRoom._type ~= "PTRoom" then
        return
    end
    -- Need to iterate through objects in reverse draw order
    for i = #_PTGlobalRenderList, 1, -1 do
        local obj = _PTGlobalRenderList[i]
        if obj.collision then
            local frame, flags = PTGetAnimationFrame(obj)
            if frame and _PTImageTestCollision(frame.ptr, mouse_x - obj.x, mouse_y - obj.y, flags) then
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
            frame, flags = PTGetAnimationFrame(obj)
            if frame and _PTImageTestCollision(frame.ptr, room_x - obj.x, room_y - obj.y, flags) then
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

local _PTUpdateRoom = function()
    if not _PTCurrentRoom or _PTCurrentRoom._type ~= "PTRoom" then
        return
    end
    for i, actor in ipairs(_PTCurrentRoom.actors) do
        if actor.moving > 0 and PTGetMillis() > actor.walkdata_next_wait then
            PTActorWalk(actor)
            PTSpriteIncrementFrame(actor.sprite)
            actor.walkdata_next_wait = PTGetMillis() + (1000 // actor.walk_rate)
            if actor == _PTCurrentRoom.camera_actor then
                _PTCurrentRoom.x, _PTCurrentRoom.y = actor.x, actor.y
            end
        end
        --print(string.format("pos: (%d, %d), walkdata_cur: (%d, %d), walkdata_next: (%d, %d), walkdata_delta_factor: (%d, %d)", actor.x, actor.y, actor.walkdata_cur.x, actor.walkdata_cur.y, actor.walkdata_next.x, actor.walkdata_next.y, actor.walkdata_delta_factor.x, actor.walkdata_delta_factor.y))
    end
    -- constrain camera to room bounds
    local x_min = _PTCurrentRoom.origin_x
    local x_max = _PTCurrentRoom.width - (SCREEN_WIDTH - _PTCurrentRoom.origin_x)
    local y_min = _PTCurrentRoom.origin_y
    local y_max = _PTCurrentRoom.height - (SCREEN_HEIGHT - _PTCurrentRoom.origin_y)
    _PTCurrentRoom.x = math.max(math.min(_PTCurrentRoom.x, x_max), x_min)
    _PTCurrentRoom.y = math.max(math.min(_PTCurrentRoom.y, y_max), y_min)
end

--- Process all input and room events.
-- @local
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
                _PTThreadsActorWait["__verb"].talk_next_wait = _PTGetMillis()
            end
        end
        ev = _PTPumpEvent()
    end
    if not _PTInputGrabbed then
        _PTUpdateMouseOver()
    end
    _PTUpdateRoom()
end

_PTRender = function()
    if not _PTCurrentRoom or _PTCurrentRoom._type ~= "PTRoom" then
        return
    end
    if _PTAutoClearScreen then
        _PTClearScreen()
    end
    for i, obj in pairs(_PTCurrentRoom.render_list) do
        if obj.visible then
            local frame, flags = PTGetAnimationFrame(obj)
            if frame then
                local tmp_x, tmp_y = PTRoomToScreen(obj.x, obj.y)
                _PTDrawImage(frame.ptr, tmp_x, tmp_y, flags)
            end
        end
    end
    if _PTWalkBoxDebug then
        for i, box in pairs(_PTCurrentRoom.boxes) do
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
    for i, obj in pairs(_PTGlobalRenderList) do
        if obj.visible then
            local frame, flags = PTGetAnimationFrame(obj)
            if frame then
                _PTDrawImage(frame.ptr, obj.x, obj.y, flags)
            end
        end
    end

    if _PTMouseSprite and not _PTInputGrabbed and _PTMouseSprite.visible then
        local mouse_x, mouse_y = _PTGetMousePos()
        local frame, flags = PTGetAnimationFrame(_PTMouseSprite)
        if frame then
            _PTDrawImage(frame.ptr, mouse_x, mouse_y, flags)
        end
    end
end
