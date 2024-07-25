PTOnlyRunOnce("test.lua");

play_sweep = function ()
    for j = 1, 20 do
        delay = 250/j;
        print(string.format("play_sweep: loop %d, delay %f\n", j, delay));
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
        print(string.format("print_crap: %d\n", i));
        PTSleep(200);
    end
end

taunt_watchdog = function ()
    limit = 100000;
    print(string.format("taunt_watchdog: I'm going to count all the way to %d, you can't stop me", limit));
    for i = 0, limit do
        if i % 1000 == 0 then
            print(string.format("taunt_watchdog: %d!", i))
        end
    end
end


-- Create a room
test_room = PTRoom("test", 320, 200);
-- Create a background image
test_img = PTImage("test.png");
test_bg = PTBackground(test_img, 32, 32, 0);
-- Assign the background image to the room
PTRoomAddObject(test_room, test_bg);



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


PTSwitchRoom(fake_room);
