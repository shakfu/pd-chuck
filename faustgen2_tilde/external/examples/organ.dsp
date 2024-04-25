
declare name "organ";
declare description "a simple additive synth";
declare author "Albert Graef";
declare version "2.0";

import("stdfaust.lib");

// master controls (volume and stereo panning)
vol = hslider("/v:[1]/vol [midi:ctrl 2] [osc:/vol]", 0.3, 0, 1, 0.01);
pan = hslider("/v:[1]/pan [midi:ctrl 10] [osc:/pan -1 1]", 0.5, 0, 1, 0.01);

// adsr controls
A = hslider("/v:[2]/[1] attack [osc:/adsr/0]", 0.01, 0, 1, 0.001); // sec
D = hslider("/v:[2]/[2] decay [osc:/adsr/1]", 0.3, 0, 1, 0.001);   // sec
S = hslider("/v:[2]/[3] sustain [osc:/adsr/2]", 0.5, 0, 1, 0.01);  // 0-1
R = hslider("/v:[2]/[4] release [osc:/adsr/3]", 0.2, 0, 1, 0.001); // sec

// relative amplitudes of the different partials
a(i) = hslider("/v:[3]/amp%i [midi:ctrl %j] [osc:/amp/%k]", 1/i, 0, 3, 0.01) with {j=i+2; k=i-1;};

// pitch bend (2 semitones up and down, in cent increments)
bend = hslider("/v:[4]/bend[midi:pitchbend]", 0, -2, 2, 0.01);

// voice controls
freq(k)	= nentry("/freq%k[voice:freq]", 440, 20, 20000, 1); // cps
gain(k)	= nentry("/gain%k[voice:gain]", 0.3, 0, 10, 0.01);  // 0-10
gate(k)	= button("/gate%k[voice:gate]");                    // 0/1

// additive synth: 3 sine oscillators with adsr envelop
voice(f) = sum(i, 3, a(i+1)*os.osc((i+1)*f*pow(2,bend/12)));

// number of voices
n = 8;

process	= sum(i, n, voice(freq(i))*(gate(i):en.adsr(A,D,S,R))*gain(i))
  : (*(vol:si.smooth(0.99)) : sp.panner(pan:si.smooth(0.99)));
