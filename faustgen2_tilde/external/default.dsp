
// Minimal Faust dsp to be used when faustgen2~ is invoked without the name of
// a dsp. You should probably leave this as is. But if you edit it, please
// make sure that the dsp compiles and runs without any hitches, since
// faustgen2~ relies on this file being present and in good working order.

import("stdfaust.lib");

process = _;
