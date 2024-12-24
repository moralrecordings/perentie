-- Create a room
test_room = PTRoom("test", SCREEN_WIDTH, SCREEN_HEIGHT)
PTSwitchRoom(test_room)

-- Add a background
background = PTBackground(PTImage("assets/stars.png"), 0, 0, -2)
PTRoomAddObject(test_room, background)

-- Set the cursor sprite
cursor_sp = PTSprite({
    PTAnimation("default", {
        PTImage("assets/cursor.png", 9, 9, 0),
    }),
    PTAnimation("selected", {
        PTImage("assets/curssel.png", 9, 9, 0),
    }),
})
PTSpriteSetAnimation(cursor_sp, "default")
PTSetMouseSprite(cursor_sp)

-- Add in the perentie head logo
logo = PTBackground(PTImage("assets/logo.png"), 16, 16, 0)
logo.hotspot_id = "logo"
logo.collision = true
logo.name = "Petra"
PTRoomAddObject(test_room, logo)

-- Load in the bitmap font
font = PTFont("assets/eagle.fnt")

-- Create some text for the header and description
version_txt = PTText(
    string.format("Perentie v%s", PTVersion()),
    font,
    200,
    "center",
    { 255, 255, 85 } -- yellow
)
version = PTBackground(version_txt, 64, 24, 0)
PTRoomAddObject(test_room, version)

description_txt = PTText(
    [[To get started, edit "main.lua".

You can open up a Lua debug shell inside the engine by connecting to COM4 with a null modem cable.
DOSBox users using the included dosbox.conf can connect with a Telnet client to TCP port 42424.

Press Q to exit.]],
    font,
    288,
    "left",
    { 255, 255, 85 } -- yellow
)
description = PTBackground(description_txt, 16, 64, 0)
PTRoomAddObject(test_room, description)

-- Add a key handler callback to quit the engine
PTOnEvent("keyDown", function(ev)
    if ev.key == "q" then
        PTQuit()
    end
end)

-- Set up mouseover text object
mouseover_text = PTBackground(nil, SCREEN_WIDTH // 2, SCREEN_HEIGHT - 16, 10)
PTGlobalAddObject(mouseover_text)

-- Add callback for when the object under the mouse changes.
-- This will run as part of the engine's event processing loop, and
-- will happen for any PTBackground/PTSprite/PTActor with the
-- collision attribute set to true.
PTOnMouseOver(function(sprite)
    if sprite and sprite.name then
        mouseover_text.image = PTText(sprite.name, font, SCREEN_WIDTH - 32, "center", { 85, 255, 85 })
        local width, height = PTGetImageDims(mouseover_text.image)
        PTSetImageOrigin(mouseover_text.image, width / 2, height)
        PTSpriteSetAnimation(cursor_sp, "selected")
    else
        mouseover_text.image = nil
        PTSpriteSetAnimation(cursor_sp, "default")
    end
end)

-- Add callback for clicking the left mouse button.
-- This will run as part of the engine's event processing loop.
PTOnEvent("mouseDown", function(event)
    local mouseover = PTGetMouseOver()
    if mouseover and mouseover.hotspot_id then
        if (event.button & 1) ~= 0 then -- left mouse button
            -- If we click on a sprite with a hotspot, call the "use" verb.
            PTDoVerb("use", mouseover.hotspot_id)
        end
    end
end)

-- Add a callback for the "use" verb on the "logo" hotspot.
-- This will run in a dedicated thread for verb actions; only
-- one can be processed at a time.
PTOnVerb("use", "logo", function()
    -- Play an arpeggio
    for i = 0, 24 do
        local freq = 220.0 * (2 ^ (i / 12.0))
        PTPlayBeep(freq)
        PTSleep(50)
        PTStopBeep()
    end
end)
