// test_msg_bind.ck -- Test for test_msg_bind.pd
//
// test_msg_bind.pd routes messages via:
//   [r option] -> [select 1 2 3 4 5]
//     option=1 -> bang           -> [s dispatch]
//     option=2 -> [msg 1.5]      -> [s dispatch]  (float)
//     option=3 -> [msg hello]    -> [s dispatch]  (symbol)
//     option=4 -> [msg list x y s 2] -> [s dispatch]
//     option=5 -> [msg foo x y z 1 2 3] -> [s dispatch]
//     else     -> [msg other]    -> [s dispatch]

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

// ---- Test: open ----
pd.open("test_msg_bind.pd", me.dir()) => int result;
assert("test_msg_bind.pd opens", result == 0);

// Bind to the dispatch receiver
pd.bind("dispatch");
50::ms => now;

// ---- Test: option=1 -> bang dispatched ----
pd.sendFloat("option", 1.0);
100::ms => now;
pd.getBang("dispatch") => int gotBang;
assert("option=1 dispatches bang", gotBang == 1);

// ---- Test: option=2 -> float 1.5 dispatched ----
pd.sendFloat("option", 2.0);
100::ms => now;
pd.getFloat("dispatch") => float gotFloat;
assert("option=2 dispatches float 1.5", gotFloat == 1.5);

// ---- Test: option=3 -> symbol "hello" dispatched ----
pd.sendFloat("option", 3.0);
100::ms => now;
pd.getSymbol("dispatch") => string gotSym;
assert("option=3 dispatches symbol hello", gotSym == "hello");

// ---- Test: else (option=99) -> symbol "other" dispatched ----
pd.sendFloat("option", 99.0);
100::ms => now;
pd.getSymbol("dispatch") => string gotOther;
assert("option=99 dispatches symbol other", gotOther == "other");

// ---- Test: sequential dispatch preserves latest value ----
pd.sendFloat("option", 2.0);  // float 1.5
100::ms => now;
pd.sendFloat("option", 1.0);  // bang (doesn't overwrite float)
100::ms => now;
pd.getFloat("dispatch") => float stillFloat;
assert("float preserved after bang dispatch", stillFloat == 1.5);

// ---- Summary ----
<<< "" >>>;
if (failures == 0)
    <<< "OK:", tests, "tests passed" >>>;
else
    <<< "FAILED:", failures, "of", tests, "tests" >>>;
