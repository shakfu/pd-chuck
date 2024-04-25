
declare name "mono";
declare description "a monophonic synth, with portamento";
declare author "Albert Graef";
declare version "2.0";

// This is basically the same as organ.dsp, but with a single voice, featuring
// legato play and portamento like ye good monophonic synths of old.

import("stdfaust.lib");

// master controls (volume and stereo panning)
vol = hslider("/v:[1]/vol [midi:ctrl 2]", 0.3, 0, 1, 0.01);
pan = hslider("/v:[1]/pan [midi:ctrl 10]", 0.5, 0, 1, 0.01);

// adsr controls
A = hslider("/v:[2]/[1] attack", 0.01, 0, 1, 0.001); // sec
D = hslider("/v:[2]/[2] decay", 0.3, 0, 1, 0.001);   // sec
S = hslider("/v:[2]/[3] sustain", 0.5, 0, 1, 0.01);  // 0-1
R = hslider("/v:[2]/[4] release", 0.2, 0, 1, 0.001); // sec

// relative amplitudes of the different partials
a(i) = hslider("/v:[3]/amp%i", 1/i, 0, 3, 0.01);

// portamento time
t   = hslider("/v:[4]/portamento [midi:ctrl 5]", 0, 0, 1, 0.01);

// pitch bend (2 semitones up and down, in cent increments)
bend = hslider("/v:[5]/bend[midi:pitchbend]", 0, -2, 2, 0.01);

// voice controls
freq	= nentry("/freq[voice:freq]", 440, 20, 20000, 1); // cps
gain	= nentry("/gain[voice:gain]", 0.3, 0, 10, 0.01);  // 0-10
gate	= button("/gate[voice:gate]");                    // 0/1

// additive synth: 3 sine oscillators with adsr envelop
voice(f) = sum(i, 3, a(i+1)*os.osc((i+1)*f*pow(2,bend/12)));

process	= voice(si.polySmooth(gate, ba.tau2pole(t/6.91), 1, freq)) *
    (gate:en.adsr(A,D,S,R))*gain
  : (*(vol:si.smooth(0.99)) : sp.panner(pan:si.smooth(0.99)));
