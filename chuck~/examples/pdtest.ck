
global float freq_m; // default 12
global float wet;

SinOsc s => JCRev r => dac;
.5 => s.gain;
.1 => r.mix;

// an array
[ 0, 2, 4, 7, 9, 11 ] @=> int hi[];

while( true )
{
    Std.mtof( 45 + Math.random2(0,3) * freq_m +
        hi[Math.random2(0,hi.size()-1)] ) => s.freq;
    wet => r.mix;

    100::ms => now;
}