
declare name "karplus";
declare description "Karplus-Strong string synth";
declare author "Yann Orlarey";
declare version "2.0";

import("stdfaust.lib");

// master volume and pan
vol	= hslider("/v:[1]/vol [midi:ctrl 2]", 0.3, 0, 1, 0.01);
pan	= hslider("/v:[1]/pan [midi:ctrl 10]", 0.5, 0, 1, 0.01);

// modulation (excitator and resonator parameters)
size	= hslider("/v:[2]/samples", 512, 1, 1024, 1); // #samples
dtime	= hslider("/v:[2]/decay time", 4, 0, 10, 0.01); // -60db decay time
// pitch bend (2 semitones up and down, in cent increments)
bend	= hslider("/v:[3]/bend[midi:pitchbend]", 0, -2, 2, 0.01);

// voice parameters
freq(i)	= nentry("/freq%i[voice:freq]", 440, 20, 20000, 1);
gain(i)	= nentry("/gain%i[voice:gain]", 1, 0, 10, 0.01);
gate(i)	= button("/gate%i[voice:gate]");

/* The excitator: */

upfront(x) 	= (x-x') > 0.0;
decay(n,x)	= x - (x>0)/n;
release(n)	= + ~ decay(n);
trigger(n) 	= upfront : release(n) : >(0.0) : +(leak);
leak		= 1.0/65536.0; // avoid denormals on Pentium
excitator	= trigger(size);

/* The resonator: */

average(x)	= (x+x')/2;
att(d,t)	= 1-1/pow(ba.db2linear(60), d/(ma.SR*t));
comb(d,a)	= (+ : de.fdelay(4096, d-1.5)) ~ (average : *(1.0-a));
resonator(d)	= comb(d,att(d,dtime));

/* DC blocker (see http://ccrma.stanford.edu/~jos/filters/DC_Blocker.html): */

dcblocker(x)	= (x-x') : (+ ~ *(0.995));

/* Karplus-Strong string synthesizer: */

smooth(c)	= *(1-c) : +~*(c);

voice(i) = no.noise*gain(i) : *(gate(i) : excitator)
	: resonator(ma.SR/(freq(i)*pow(2,bend/12)))
	: dcblocker;

n = 8;
process	= sum(i, n, voice(i))
	: (*(smooth(0.99, vol)) : sp.panner(smooth(0.99, pan)));
