import("stdfaust.lib");

// square signal (1/0), changing state at each received clock
clocker = checkbox("MIDI clock [midi:clock]");    

// ON/OFF button controlled with MIDI start/stop messages
play = checkbox("ON/OFF [midi:start] [midi:stop]");    

// detect front
front(x) = (x-x') != 0.0;      

// count number of peaks during one second
freq(x) = (x-x@ma.SR) : + ~ _;   
   
process = os.osc(8*freq(front(clocker))) * play;
