PTOnlyRunOnce("test.lua");

play_sweep = function ()
    for j = 1, 10 do
        delay = 250/j;
        _PTLog(string.format("play_sweep: loop %d, delay %f\n", j, delay));
        for i = 0, 24 do 
            freq = 220.0 * (2 ^ (i/12.0));
            PTPlayBeep(freq);
            PTSleep(delay);
            PTStopBeep();
        end
    end
end

print_crap = function ()
    for i = 1, 100 do
        _PTLog(string.format("print_crap: %d\n", i));
        PTSleep(200);
    end
end

taunt_watchdog = function ()
    limit = 100000;
    _PTLog(string.format("taunt_watchdog: I'm going to count all the way to %d, you can't stop me", limit));
    for i = 0, limit do
        if i % 1000 == 0 then
            _PTLog(string.format("taunt_watchdog: %d!", i))
        end
    end
end


-- Create a room
test_room = PTRoom("test", 320, 200);




-- the C blitter would then zero the screen,
-- take the current room
-- and iterate through all the backgrounds/sprites
-- and draw them to the screen.
-- bonus would be for each item to have a dirty flag,
-- and keep track of each change with dirty rectangles.

fake_room = PTRoom("texttest", 0, 0);

PTOnRoomEnter("texttest", function ()
    PTStartThread("play_sweep", play_sweep);
    PTStartThread("print_crap", print_crap);
    PTStartThread("taunt_watchdog", taunt_watchdog);
end);

PTOnRoomExit("texttest", function ()
    PTStopThread("play_sweep");
    PTStopThread("print_crap");
end);

graphics_room = PTRoom("graphics", 320, 200);
-- Create a background image
test_img = PTImage("test.png");
test_bg = PTBackground(test_img, 32, 32, 0);

cursor_img = PTImage("cursor.png", 8, 8);
cursor_sp = PTBackground(cursor_img, 0, 0, 0);
PTSetMouseSprite(cursor_sp);

-- Assign the background image to the room
PTRoomAddObject(graphics_room, test_bg);
_PTLog(tostring(graphics_room));

draw_things = function ()
    for i = 1, 1000 do
        test_bg.x = math.random(-64, 320);
        test_bg.y = math.random(-64, 200);
        PTSleep(20);
    end
end


PTOnRoomEnter("graphics", function ()
    PTStartThread("play_sweep", play_sweep);
    PTStartThread("draw_things", draw_things);
end);

PTSwitchRoom(graphics_room);
