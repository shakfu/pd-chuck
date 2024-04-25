
/* Stereo chorus. */

declare name "chorus";
declare description "stereo chorus effect";
declare author "Albert Graef";
declare version "2.0";

import("stdfaust.lib");

level	= hslider("level [midi:ctrl 93]", 0.5, 0, 1, 0.01);
freq	= hslider("freq", 3, 0, 10, 0.01);
dtime	= hslider("delay", 0.025, 0, 0.2, 0.001);
depth	= hslider("depth", 0.02, 0, 1, 0.001);

tblosc(n,f,freq,mod)	= (1-d)*rdtable(n,wave,i&(n-1)) +
			  d*rdtable(n,wave,(i+1)&(n-1))
with {
	wave	 	= ba.time*(2.0*ma.PI)/n : f;
	phase		= freq/ma.SR : (+ : ma.frac) ~ _;
	modphase	= ma.frac(phase+mod/(2*ma.PI))*n;
	i		= int(floor(modphase));
	d		= ma.frac(modphase);
};

chorus(dtime,freq,depth,phase,x)
			= x+level*de.fdelay(1<<16, t, x)
with {
	t		= ma.SR*dtime/2*(1+depth*tblosc(1<<16, sin, freq, phase));
};

process			= (left, right)
with {
	left		= chorus(dtime,freq,depth,0);
	right		= chorus(dtime,freq,depth,ma.PI/2);
};
