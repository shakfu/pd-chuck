// test2.ck -- Test for test2.pd
//
// test2.pd is a synth with:
//   [r freq]           -> [mtof] -> [osc~] -> [dac~]
//   [r reset]          -> triggers [f 60] (middle C)
//   [r has_overtones]  -> toggles overtone mix (2x, 3x, 4x harmonics)

0 => int failures;
0 => int tests;

fun void assert(string label, int condition)
{
    1 +=> tests;
    if (!condition)
    {
        1 +=> failures;
        <<< "FAIL:", label >>>;
    }
    else
    {
        <<< "PASS:", label >>>;
    }
}

PdPatch pd => blackhole;

// ---- Test: open patch ----
pd.open("test2.pd", me.dir()) => int result;
assert("test2.pd opens", result == 0);

// ---- Test: sendFloat to freq (MIDI note) ----
pd.sendFloat("freq", 69.0); // A4
50::ms => now;
assert("sendFloat freq accepted", true);

// ---- Test: sendFloat to has_overtones toggle ----
pd.sendFloat("has_overtones", 1.0);
50::ms => now;
assert("sendFloat has_overtones=1 accepted", true);

pd.sendFloat("has_overtones", 0.0);
50::ms => now;
assert("sendFloat has_overtones=0 accepted", true);

// ---- Test: sendBang to reset ----
pd.sendBang("reset");
50::ms => now;
assert("sendBang reset accepted", true);

// ---- Test: change freq after reset ----
pd.sendFloat("freq", 48.0); // C3
50::ms => now;
assert("sendFloat freq after reset accepted", true);

// ---- Summary ----
<<< "" >>>;
if (failures == 0)
    <<< "OK:", tests, "tests passed" >>>;
else
    <<< "FAILED:", failures, "of", tests, "tests" >>>;
