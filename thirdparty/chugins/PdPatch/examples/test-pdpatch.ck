// test-pdpatch.ck -- Functional test for PdPatch chugin
//
// Tests: open, close, sendFloat, sendBang, sendSymbol,
//        bind, getFloat, getBang, getSymbol, arraySize
//
// Uses test-pdpatch.pd which contains:
//   [r freq]      -> [osc~] -> [*~ 0.3] -> [dac~]
//   [r test-in]   -> [* 2]  -> [s test-out]     (doubles input float)
//   [r get-bang]   -> [b]    -> [s bang-out]     (echoes bang)
//   [r get-symbol] ->          [s symbol-out]    (echoes symbol)

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

// ---- Test 1: Construction and open ----
PdPatch pd => blackhole;

pd.open("test-pdpatch.pd", me.dir()) => int result;
assert("open returns 0", result == 0);

// ---- Test 2: sendFloat and receive float ----
pd.bind("test-out");

pd.sendFloat("test-in", 21.0);
// Advance time so Pd processes the message
100::ms => now;

pd.getFloat("test-out") => float doubled;
assert("sendFloat/getFloat: 21*2 == 42", doubled == 42.0);

// ---- Test 3: sendBang and receive bang ----
pd.bind("bang-out");

pd.sendBang("get-bang");
100::ms => now;

pd.getBang("bang-out") => int bangReceived;
assert("sendBang/getBang: received", bangReceived == 1);

// getBang should be consumed (returns 0 on second call)
pd.getBang("bang-out") => int bangConsumed;
assert("getBang consumed after read", bangConsumed == 0);

// ---- Test 4: sendSymbol and receive symbol ----
pd.bind("symbol-out");

pd.sendSymbol("get-symbol", "hello");
100::ms => now;

pd.getSymbol("symbol-out") => string sym;
assert("sendSymbol/getSymbol: hello", sym == "hello");

// ---- Test 5: close and reopen ----
pd.close();

pd.open("test-pdpatch.pd", me.dir()) => int result2;
assert("reopen returns 0", result2 == 0);

// ---- Test 6: open failure ----
PdPatch pd2 => blackhole;
pd2.open("nonexistent.pd", me.dir()) => int badResult;
assert("open nonexistent returns -1", badResult == -1);

// ---- Test 7: unbind ----
pd.unbind("test-out");
pd.sendFloat("test-in", 99.0);
100::ms => now;
// After unbind, getFloat should return 0 (default, no longer tracked)
pd.getFloat("test-out") => float afterUnbind;
assert("getFloat after unbind returns 0", afterUnbind == 0.0);

// ---- Test 8: multiple instances ----
PdPatch pd3 => blackhole;
pd3.open("test-pdpatch.pd", me.dir()) => int r3;
assert("second instance opens ok", r3 == 0);

pd3.bind("test-out");
pd3.sendFloat("test-in", 10.0);
100::ms => now;
pd3.getFloat("test-out") => float inst2val;
assert("second instance independent messaging", inst2val == 20.0);

// ---- Summary ----
<<< "" >>>;
if (failures == 0)
{
    <<< "OK:", tests, "tests passed" >>>;
}
else
{
    <<< "FAILED:", failures, "of", tests, "tests" >>>;
}
