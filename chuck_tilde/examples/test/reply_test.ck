global int counter;
global float freq;
global Event tick;
440.0 => freq;
7 => counter;
while (true) { counter + 1 => counter; tick.broadcast(); 200::ms => now; }
