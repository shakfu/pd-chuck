
/* Stereo amplifier stage with bass, treble, gain and balance controls and a
   dB meter. */

declare name "amp";
declare description "stereo amplifier stage";
declare author "Albert Graef";
declare version "2.0";

import("stdfaust.lib");

/* Fixed bass and treble frequencies. You might want to tune these for your
   setup. */

bass_freq	= 300;
treble_freq	= 1200;

/* Smoothing (lowpass) filter from signals.lib. We use this for the gain and
   balance controls to avoid zipper noise. */

smooth = si.smooth(0.99);

/* Bass and treble gain controls in dB. The range of +/-20 corresponds to a
   boost/cut factor of 10. */

bass_gain	= nentry("[1] bass [midi:ctrl 16] [unit:dB]", 0, -20, 20, 0.1);
treble_gain	= nentry("[2] treble [midi:ctrl 17] [unit:dB]", 0, -20, 20, 0.1);

/* Gain and balance controls. */

gain		= smooth(ba.db2linear(g)) with {
  g = nentry("[3] gain [midi:ctrl 7] [unit:dB]", 0, -60, 10, 0.1);
};
bal		= smooth(b) with {
  b = hslider("balance [midi:ctrl 8]", 0, -1, 1, 0.001);
};

/* Balance a stereo signal using the constant power pan rule. Note that this
   will attenuate the signal by 3 dB in the center position. If you prefer,
   you can just apply a makeup gain of sqrt(2) to have unity gain in the
   center position, but this will also boost the signal by the same amount
   (possibly causing distortion) when panning hard left or right. */

balance		= *(l), *(r) with {
  p = ma.PI*(bal+1)/4; l = cos(p); r = sin(p);
};

/* Generic biquad filter. */

filter(b0,b1,b2,a0,a1,a2)	= f : (+ ~ g)
with {
	f(x)	= (b0/a0)*x+(b1/a0)*x'+(b2/a0)*x'';
	g(y)	= 0-(a1/a0)*y-(a2/a0)*y';
};

/* Low and high shelf filters, straight from Robert Bristow-Johnson's "Audio
   EQ Cookbook", see http://www.musicdsp.org/files/Audio-EQ-Cookbook.txt. f0
   is the shelf midpoint frequency, g the desired gain in dB. S is the shelf
   slope parameter, we always set that to 1 here. */

low_shelf(f0,g)		= filter(b0,b1,b2,a0,a1,a2)
with {
	S  = 1;
	A  = pow(10,g/40);
	w0 = 2*ma.PI*f0/ma.SR;
	alpha = sin(w0)/2 * sqrt( (A + 1/A)*(1/S - 1) + 2 );

	b0 =    A*( (A+1) - (A-1)*cos(w0) + 2*sqrt(A)*alpha );
	b1 =  2*A*( (A-1) - (A+1)*cos(w0)                   );
	b2 =    A*( (A+1) - (A-1)*cos(w0) - 2*sqrt(A)*alpha );
	a0 =        (A+1) + (A-1)*cos(w0) + 2*sqrt(A)*alpha;
	a1 =   -2*( (A-1) + (A+1)*cos(w0)                   );
	a2 =        (A+1) + (A-1)*cos(w0) - 2*sqrt(A)*alpha;
};

high_shelf(f0,g)	= filter(b0,b1,b2,a0,a1,a2)
with {
	S  = 1;
	A  = pow(10,g/40);
	w0 = 2*ma.PI*f0/ma.SR;
	alpha = sin(w0)/2 * sqrt( (A + 1/A)*(1/S - 1) + 2 );

	b0 =    A*( (A+1) + (A-1)*cos(w0) + 2*sqrt(A)*alpha );
	b1 = -2*A*( (A-1) + (A+1)*cos(w0)                   );
	b2 =    A*( (A+1) + (A-1)*cos(w0) - 2*sqrt(A)*alpha );
	a0 =        (A+1) - (A-1)*cos(w0) + 2*sqrt(A)*alpha;
	a1 =    2*( (A-1) - (A+1)*cos(w0)                   );
	a2 =        (A+1) - (A-1)*cos(w0) - 2*sqrt(A)*alpha;
};

/* The tone control. We simply run a low and a high shelf in series here. */

tone		= low_shelf(bass_freq,bass_gain)
		: high_shelf(treble_freq,treble_gain);

/* Envelop follower. This is basically a 1 pole LP with configurable attack/
   release time. The result is converted to dB. You have to set the desired
   attack/release time in seconds using the t parameter below. */

t		= 0.1;			// attack/release time in seconds
g		= exp(-1/(ma.SR*t));	// corresponding gain factor

env		= abs : *(1-g) : + ~ *(g) : ba.linear2db;

/* Use this if you want the RMS instead. Note that this doesn't really
   calculate an RMS value (you'd need an FIR for that), but in practice our
   simple 1 pole IIR filter works just as well. */

rms		= sqr : *(1-g) : + ~ *(g) : sqrt : ba.linear2db;
sqr(x)		= x*x;

/* The dB meters for left and right channel. These are passive controls. */

left_meter(x)	= attach(x, env(x) : hbargraph("left [midi:ctrl 18] [osc:/left -60 10] [unit:dB]", -60, 10));
right_meter(x)	= attach(x, env(x) : hbargraph("right [midi:ctrl 19] [osc:/right -60 10] [unit:dB]", -60, 10));

/* The main program. */

process		= hgroup("[1]", (tone, tone) : (_*gain, _*gain))
		: vgroup("[2]", balance)
		: vgroup("[3]", (left_meter, right_meter));
