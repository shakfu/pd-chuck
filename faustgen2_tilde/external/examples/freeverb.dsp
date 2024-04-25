
import("stdfaust.lib");

freeverb = _,_ <: (*(g)*fixedgain,*(g)*fixedgain :
	re.stereo_freeverb(combfeed, allpassfeed, damping, spatSpread)),
	*(1-g), *(1-g) :> _,_
with{
	scaleroom   = 0.28;
	offsetroom  = 0.7;
	allpassfeed = 0.5;
	scaledamp   = 0.4;
	fixedgain   = 0.1;
	origSR = 44100;

	parameters(x) = vgroup("freeverb",x);
	knobGroup(x) = parameters(vgroup("[0]",x));
	damping = knobGroup(hslider("[0] damp",0.5, 0, 1, 0.025)*scaledamp*origSR/ma.SR);
	combfeed = knobGroup(hslider("[1] room size", 0.5, 0, 1, 0.025)*scaleroom*origSR/ma.SR + offsetroom);
	spatSpread = knobGroup(hslider("[2] stereo spread",0.5,0,1,0.01)*46*ma.SR/origSR : int);
	g = parameters(hslider("[1] wet [midi:ctrl 91]", 0.3333, 0, 1, 0.025));
};

process = freeverb;
