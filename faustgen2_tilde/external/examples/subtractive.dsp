
declare name "subtractive";
declare description "saw wave filtered with resonant lowpass";
declare author "Albert Graef";
declare version "2.0";

import("stdfaust.lib");

nvoices = 8;

process = sum(i, nvoices, voice(i))
  : (*(vol:dezipper(0.99))  : sp.panner(pan:dezipper(0.99)))
with {
  env(i) = gate(i) : en.adsr(attack, decay, sustain, release);
  filter(env,freq) =
    lowpass(env*res, ma.fmax(1/cutoff, env)*freq*cutoff);
  dezipper(c) = *(1-c) : +~*(c);
  voice(i) = os.sawtooth(freq(i))
  : ((env(i),freq(i),_) : filter) : *(env(i) * gain(i));
};

// control variables

vol	= hslider("[1] vol [midi:ctrl 2]", 0.3, 0, 1, 0.01);
pan	= hslider("[2] pan [midi:ctrl 10]", 0.5, 0, 1, 0.01);

attack	= hslider("[3] attack", 0.01, 0, 1, 0.001);
decay	= hslider("[4] decay", 0.3, 0, 1, 0.001);
sustain = hslider("[5] sustain", 0.5, 0, 1, 0.01);
release = hslider("[6] release", 0.2, 0, 1, 0.001);

res	= hslider("[7] resonance (dB) [midi:ctrl 75]", 3, 0, 20, 0.1);
cutoff	= hslider("[8] cutoff (harmonic) [midi:ctrl 74]", 6, 1, 20, 0.1);

// voice controls
freq(i)	= nentry("freq%i[voice:freq]", 440, 20, 20000, 1);
gain(i)	= nentry("gain%i[voice:gain]", 1, 0, 10, 0.01);
gate(i)	= button("gate%i[voice:gate]");

// Tweaked Butterworth filter by David Werner and Patrice Tarrabia,
// see http://www.musicdsp.org/showArchiveComment.php?ArchiveID=180.
lowpass(res,freq) = f : (+ ~ g) : *(a) with {
  f(x)	= a0*x+a1*x'+a2*x'';
  g(y)	= -b1*y-b2*y';
  // calculate the filter coefficients
  a	= 1/ba.db2linear(0.5*res);
  c	= 1/tan(ma.PI*(freq/ma.SR));
  c2	= c*c;
  r	= 1/ba.db2linear(2*res);
  q	= sqrt(2)*r;
  a0	= 1/(1+(q*c)+(c2));
  a1	= 2*a0;
  a2	= a0;
  b1	= 2*a0*(1-c2);
  b2	= a0*(1-q*c+c2);
};
