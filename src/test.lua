PTOnlyRunOnce("test.lua");

play_sweep = function ()
    for j = 1, 20 do
        delay = 250/j;
        print(string.format("play_sweep: loop %d, delay %d\n", j, delay));
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


PTOnRoomEnter("test", function ()
    PTStartThread("play_sweep", play_sweep);
    PTStartThread("print_crap", print_crap);
end);

PTOnRoomExit("test", function ()
    PTStopThread("play_sweep");
    PTStopThread("print_crap");
end);


PTSwitchRoom("test");
