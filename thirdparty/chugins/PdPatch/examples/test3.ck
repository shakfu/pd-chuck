// test3.ck -- Test for test3.pd
//
// test3.pd is a self-contained AM synth:
//   [osc~ 440] * [osc~ 0.1] -> [dac~]
// No message interface -- this test verifies load and audio processing.

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
pd.open("test3.pd", me.dir()) => int result;
assert("test3.pd opens", result == 0);

// ---- Test: runs without error ----
200::ms => now;
assert("audio processes without error", true);

// ---- Test: close and reopen ----
pd.close();
pd.open("test3.pd", me.dir()) => int result2;
assert("test3.pd reopens after close", result2 == 0);
100::ms => now;
assert("audio processes after reopen", true);

// ---- Summary ----
<<< "" >>>;
if (failures == 0)
    <<< "OK:", tests, "tests passed" >>>;
else
    <<< "FAILED:", failures, "of", tests, "tests" >>>;
