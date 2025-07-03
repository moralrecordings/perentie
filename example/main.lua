-- Define the game metadata.
-- This should be the first line of your main.lua
PTSetGameInfo("au.net.moral.perentie.example", "1.0.0", "Perentie - Example app")

-- Get size of the screen
SCREEN_WIDTH, SCREEN_HEIGHT = PTGetScreenDims()

-- Create a room.
-- A room is the basic scene structure used in Perentie. They are a fixed size,
-- and keep track of objects in a scene graph.
-- Changing the x and y properties of the room will move the camera position.
-- Add the room to the engine, and switch the current focus to it.
test_room = PTRoom("test", SCREEN_WIDTH, SCREEN_HEIGHT)
PTAddRoom(test_room)
PTSwitchRoom("test")

-- Create a background, add it to the room.
-- Because we're adding it to a room, the coordinates are in room-space.
background = PTBackground(PTImage("assets/stars.png"), 0, 0, -2)
PTRoomAddObject(test_room, background)

-- Create a sprite for the cursor.
-- This has two animations: one for the default state, and one for hovering over an hotspot.
-- For this we have loaded in two PNG files from the filesystem, and set the image origin to (9, 9);
-- this will position the center of the crosshair image at the sprite coordinates.
cursor_sp = PTSprite({
    PTAnimation("default", {
        PTImage("assets/cursor.png", 9, 9, 0),
    }),
    PTAnimation("selected", {
        PTImage("assets/curssel.png", 9, 9, 0),
    }),
})
PTSpriteSetAnimation(cursor_sp, "default")
-- Tell the engine to render this sprite at the mouse coordinates.
PTSetMouseSprite(cursor_sp)

-- Create a background image with the perentie head logo.
-- Set the collision property (used to enable PTOnMouseOver events),
-- along with some other properties used by our later event code.
-- Add this image to the room.
logo = PTBackground(PTImage("assets/logo.png"), 16, 16, 0)
logo.collision = true
logo.hotspot_id = "logo"
logo.name = "Petra"
PTRoomAddObject(test_room, logo)

-- Load in the bitmap fonts
title_font = PTFont("assets/eagle.fnt")
body_font = PTFont("assets/tiny.fnt")

-- Create some text for the header and description.
-- PTText will output a static image, the same type as PTImage. You can specify the
-- font, maximum vertical width, horizontal alignment and colouring.
-- Place this in a PTBackground container and add it to the room scene graph.
version_txt = PTText(
    string.format("Perentie v%s", PTVersion()),
    title_font,
    200,
    "center",
    { 255, 255, 85 } -- yellow
)
version = PTBackground(version_txt, 64, 24, 0)
PTRoomAddObject(test_room, version)

-- Do the same for the big description text.
description_txt = PTText(
    [[To get started, edit "main.lua".

You can open up a Lua debug shell inside the engine by connecting to COM4 with a null modem cable.
DOSBox users using the included dosbox.conf can connect with a Telnet client to TCP port 42424.

Press R to hot reload.
Press Q to exit.]],
    body_font,
    288,
    "left",
    { 255, 255, 85 } -- yellow
)
description = PTBackground(description_txt, 16, 64, 0)
PTRoomAddObject(test_room, description)

-- Set up a background image for showing hover text.
-- Unlike the previous images, add this to the global scene graph.
-- This means the coordinates are in screen-space, and this will always
-- render in front of the room graphics.
mouseover_text = PTBackground(nil, SCREEN_WIDTH // 2, SCREEN_HEIGHT - 16, 10)
PTGlobalAddObject(mouseover_text)

-- All of the above code is run once when you start the engine.
-- Now for code that runs when the game is played, we need to use callbacks.

-- Add a callback that runs whenever a key is pressed.
-- This will run as part of the engine's event processing loop.
PTOnEvent("keyDown", function(ev)
    if ev.key == "q" then
        -- Quit the engine
        PTQuit()
    elseif ev.key == "r" then
        -- Hot reload the engine.
        -- This will save the current engine state to slot 0,
        -- then load the engine state.
        -- In practice, loading triggers a reset (much like PTReset):
        -- - the Lua environment is cleared
        -- - the game's Lua code is reloaded from the disk
        -- - variables and attributes from the state file are overlayed
        PTSaveState(0)
        PTLoadState(0)
    end
end)

-- Add a callback for when the object under the mouse changes.
-- This will run as part of the engine's event processing loop, and
-- will fire for any PTBackground/PTSprite/PTActor in global or room space
-- with the collision property set to true.
PTOnMouseOver(function(sprite)
    if sprite and sprite.name then
        mouseover_text.image = PTText(sprite.name, title_font, SCREEN_WIDTH - 32, "center", { 85, 255, 85 })
        local width, height = PTGetImageDims(mouseover_text.image)
        PTSetImageOrigin(mouseover_text.image, width / 2, height)
        PTSpriteSetAnimation(cursor_sp, "selected")
    else
        mouseover_text.image = nil
        PTSpriteSetAnimation(cursor_sp, "default")
    end
end)

-- Add a callback for clicking the left mouse button.
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
        PTPCSpeakerPlayTone(freq)
        PTSleep(50)
        PTPCSpeakerStop()
    end
end)

-- Enable the debug terminal.
-- Don't add this for your release builds, it can cause issues on real DOS hardware.
PTSetDebugConsole(true, "COM4")
