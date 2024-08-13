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
    return {
        _type = "PTRoom",
        name = name,
        width = width,
        height = height,
        render_list = {},
        boxes = {},
        box_matrix = { {} },
        actors = {},
    }
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

PTRoomSetWalkBoxes = function(room, boxes)
    if not room or room._type ~= "PTRoom" then
        error("PTRoomSetWalkboxes: expected PTRoom for first argument")
    end
    for i = 1, #boxes do
        if not boxes[i] or boxes[i]._type ~= "PTWalkBox" then
            error("PTRoomSetWalkboxes: expected an array of PTWalkBox for second argument")
        end
        boxes[i].id = i
    end
    room.boxes = boxes
    room.box_links = PTGenLinksFromWalkBoxes(boxes)
    room.box_matrix = PTGenWalkBoxMatrix(#boxes, room.box_links)
end

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
    target = room.box_matrix[from_id][to_id]
    if target == 0 then
        return to_id
    end
    return room.boxes[target]
end

----- walkbox code -----

-- algorithms nicked from ScummVM's SCUMM engine implementation

MF_NEW_LEG = 1
MF_IN_LEG = 2
MF_TURN = 4
MF_LAST_LEG = 8

PTPoint = function(x, y)
    return { x = x, y = y }
end

PTWalkBox = function(ul, ur, lr, ll, z)
    return { _type = "PTWalkBox", id = nil, ul = ul, ur = ur, lr = lr, ll = ll, z = z }
end

local _PTStraightLinesOverlap = function(a1, a2, b1, b2)
    if a1.x == a2.x and b1.x == b2.x and a1.x == b1.x then
        -- vertical line
        return a1.y <= b2.y and b1.y <= a2.y
    end
    if a1.y == a2.y and b1.y == b2.y and a1.y == b1.y then
        -- horizontal line
        return a1.x <= b2.x and b1.x <= a2.x
    end
    return false
end

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

_PTNewMatrix = function(n)
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

_PTCopyMatrix = function(mat)
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
    if dist < best_dist then
        best_dist = dist
        result = tmp
    end
    tmp = _PTClosestPointOnLine(box.lr, box.ll, point)
    if dist < best_dist then
        best_dist = dist
        result = tmp
    end
    tmp = _PTClosestPointOnLine(box.ll, box.ul, point)
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
    return best_point, best_box
end

local _PTFindPathTowards = function(actor, b1, b2, b3)
    local box1 = PTWalkBox(b1.ul, b1.ur, b1.lr, b1.ll)
    local box2 = PTWalkBox(b2.ul, b2.ur, b2.lr, b2.ll)
    local found_path = PTPoint(0, 0)
    for i = 1, 4 do
        for j = 1, 4 do
            -- if the top line has the same x coordinate
            if box1.ul.x == box1.ur.x and box1.ul.x == box2.ul.x and box1.ul.x == box2.ur.x then
                local flag = 0
                -- switch coordinates if not ordered
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
                    -- switch coordinates back if required
                    if flag & 1 > 0 then
                        box1.ul, box1.ur = PTPoint(box1.ul.x, box1.ur.y), PTPoint(box1.ur.x, box1.ul.y)
                    end
                    if flag & 2 > 0 then
                        box2.ul, box2.ur = PTPoint(box2.ul.x, box2.ur.y), PTPoint(box2.ur.x, box2.ul.y)
                    end
                else
                    local pos_y = actor.y
                    if b2.id == b3.id then
                        local diff_x = actor.walkdata_dest.x - actor.x
                        local diff_y = actor.walkdata_dest.y - actor.y
                        local box_diff_x = box1.ul.x - actor.x
                        if diff_x ~= 0 then
                            diff_y = diff_y * box_diff_x
                            local t = diff_y // diff_x
                            if t == 0 and (diff_y <= 0 or diff_x <= 0) and (diff_y >= 0 or diff_x >= 0) then
                                t = -1
                            end
                            pos_y = actor.y + t
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
                    found_path = PTPoint(box1.ul.x, q)
                    return false, found_path
                end
            end
            -- if the top line has the same y coordinate
            if box1.ul.y == box1.ur.y and box1.ul.y == box2.ul.y and box1.ul.y == box2.ur.y then
                local flag = 0
                -- switch coordinates if not ordered
                if box1.ul.x > box1.ur.x then
                    box1.ul, box1.ur = PTPoint(box1.ul.x, box1.ur.y), PTPoint(box1.ur.x, box1.ul.y)
                    flag = flag | 1
                end
                if box2.ul.x > box2.ur.x then
                    box2.ul, box2.ur = PTPoint(box2.ul.x, box2.ur.y), PTPoint(box2.ur.x, box2.ul.y)
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
                    -- switch coordinates back if required
                    if flag & 1 > 0 then
                        box1.ul, box1.ur = PTPoint(box1.ul.x, box1.ur.y), PTPoint(box1.ur.x, box1.ul.y)
                    end
                    if flag & 2 > 0 then
                        box2.ul, box2.ur = PTPoint(box2.ul.x, box2.ur.y), PTPoint(box2.ur.x, box2.ul.y)
                    end
                else
                    local pos_x = actor.x
                    if b2.id == b3.id then
                        local diff_x = actor.walkdata_dest.x - actor.x
                        local diff_y = actor.walkdata_dest.y - actor.y
                        local box_diff_y = box1.ul.y - actor.y
                        if diff_y ~= 0 then
                            pos_x = pos_x + (diff_x * box_diff_y // diff_y)
                        end
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
        box2.ul = box1.ur
        box2.ur = box1.lr
        box2.lr = box1.ll
        box2.ll = tmp
    end
    return false, found_path
end

local _PTActorWalkStep = function(actor)
    if actor.walkbox.id ~= actor.walkdata_curbox.id and _PTCheckPointInBoxBounds(actor, actor.walkdata_curbox) then
        PTActorSetWalkBox(actor, actor.walkdata_curbox)
    end
    local dist_x = math.abs(actor.walkdata_next.x - actor.walkdata_cur.x)
    local dist_y = math.abs(actor.walkdata_next.y - actor.walkdata_cur.y)
    if math.abs(actor.x - actor.walkdata_cur.x) >= dist_x and math.abs(actor.y - actor.walkdata_cur.y) >= dist_y then
        return false
    end

    local tmp_x = (actor.x << 16) + actor.walkdata_frac.x + (actor.walkdata_delta_factor.x >> 8) * actor.scale_x
    local tmp_y = (actor.y << 16) + actor.walkdata_frac.y + (actor.walkdata_delta_factor.y >> 8) * actor.scale_y
    actor.walkdata_frac = PTPoint(tmp_x & 0xffff, tmp_y & 0xffff)
    actor.x, actor.y = tmp_x >> 16, tmp_y >> 16
    if math.abs(actor.x - actor.walkdata_cur.x) > dist_x then
        actor.x = actor.walkdata_next.x
    end
    if math.abs(actor.y - actor.walkdata_cur.y) > dist_y then
        actor.y = actor.walkdata_next.y
    end
    return true
end

local _PTCalcMovementFactor = function(actor, next)
    if actor.x == next.x and actor.y == next.y then
        return false
    end

    local diff_x = next.x - actor.x
    local diff_y = next.y - actor.y
    local delta_y_factor = actor.speed_y << 16
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
        delta_x_factor = actor.speed_x << 16
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

    actor.walkdata_frac = PTPoint(0, 0)
    actor.walkdata_cur = PTPoint(actor.x, actor.y)
    actor.walkdata_next = next
    actor.walkdata_delta_factor = PTPoint(delta_x_factor, delta_y_factor)
    actor.walkdata_facing = (math.floor(math.atan(delta_x_factor, -delta_y_factor) * 180 / math.pi) + 360) % 360
    return _PTActorWalkStep(actor)
end

--- Set the actor moving towards a point in the room.
-- @tparam table actor Actor to modify.
-- @tparam int x X coordinate in room space.
-- @tparam int y Y coordinate in room space.
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

PTActorWalk = function(actor)
    if not actor or actor._type ~= "PTActor" then
        error("PTActorWalk: expected PTActor for first argument")
    end

    if actor.moving == 0 then
        return
    end

    if actor.moving ~= MF_NEW_LEG then
        if _PTActorWalkStep(actor) then
            return
        end
        if actor.moving == MF_LAST_LEG then
            actor.moving = 0
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
        actor.walkdata_curbox = next_box

        local result, found_path = _PTFindPathTowards(actor, actor.walkbox, next_box, actor.walkdata_destbox)
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

----- actor code -----

PTActor = function(name)
    return {
        _type = "PTActor",
        name = name,
        x = 0,
        y = 0,
        z = 0,
        visible = true,
        room = nil,
        sprite = nil,
        talk_x = 0,
        talk_y = 0,
        talk_img = nil,
        talk_font = nil,
        talk_color = { 0xff, 0xff, 0xff },
        talk_millis = nil,
        walkdata_dest = PTPoint(0, 0),
        walkdata_cur = PTPoint(0, 0),
        walkdata_next = PTPoint(0, 0),
        walkdata_frac = PTPoint(0, 0),
        walkdata_delta_factor = PTPoint(0, 0),
        walkdata_destbox = nil,
        walkdata_curbox = nil,
        walkbox = nil,
        scale_x = 0xff,
        scale_y = 0xff,
        speed_x = 8,
        speed_y = 2,
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
    if box.z then
        actor.z = box.z
    end
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

PTActorUpdate = function(actor, fast_forward)
    if not actor or actor._type ~= "PTActor" then
        error("PTActorUpdate: expected PTActor for first argument")
    end
    if actor.talk_img and actor.talk_millis then
        if not fast_forward and _PTGetMillis() < actor.talk_millis then
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

----- graphics code -----

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
        current_animation = nil,
        collision = false,
        visible = true,
    }
end

PTAnimation = function(rate, frames)
    return {
        _type = "PTAnimation",
        rate = rate,
        frames = frames,
        looping = false,
        bounce = false,
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
    elseif object and object._type == "PTActor" then
        return PTGetAnimationFrame(object.sprite)
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

_PTUpdateRoom = function()
    if not _PTCurrentRoom or _PTCurrentRoom._type ~= "PTRoom" then
        return
    end
    for i, actor in ipairs(_PTCurrentRoom.actors) do
        PTActorWalk(actor)
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
            frame = PTGetAnimationFrame(obj)
            if frame then
                _PTDrawImage(frame.ptr, obj.x, obj.y)
            end
        end
    end
    for i, obj in pairs(_PTGlobalRenderList) do
        if obj.visible then
            frame = PTGetAnimationFrame(obj)
            if frame then
                _PTDrawImage(frame.ptr, obj.x, obj.y)
            end
        end
    end

    if _PTMouseSprite and not _PTInputGrabbed and _PTMouseSprite.visible then
        mouse_x, mouse_y = _PTGetMousePos()
        frame = PTGetAnimationFrame(_PTMouseSprite)
        if frame then
            _PTDrawImage(frame.ptr, mouse_x, mouse_y)
        end
    end
end
