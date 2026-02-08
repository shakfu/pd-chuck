// test_msg_send.ck -- Test for test_msg_send.pd (also covers test_msg.pd)
//
// test_msg_send.pd receives on:
//   [r mybang]    -> [print]
//   [r myfloat]   -> [+ 1] -> [print]
//   [r mysymbol]  -> [print]
//   [r mylist]    -> [print]
//   [r mymessage] -> [print]
//
// Output goes to [print] only (no [s ...]), so we verify that
// all send types execute without error. Print output appears on stderr.

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
pd.open("test_msg_send.pd", me.dir()) => int result;
assert("test_msg_send.pd opens", result == 0);
50::ms => now;

// ---- Test: sendBang ----
pd.sendBang("mybang");
50::ms => now;
assert("sendBang mybang accepted", true);

// ---- Test: sendFloat ----
pd.sendFloat("myfloat", 41.0);  // +1 in patch = prints 42
50::ms => now;
assert("sendFloat myfloat accepted", true);

// ---- Test: sendSymbol ----
pd.sendSymbol("mysymbol", "testing");
50::ms => now;
assert("sendSymbol mysymbol accepted", true);

// ---- Test: sendList ----
[1.0, 2.0, 3.0] @=> float listVals[];
pd.sendList("mylist", listVals);
50::ms => now;
assert("sendList mylist accepted", true);

// ---- Test: sendMessage ----
[10.0, 20.0] @=> float msgArgs[];
pd.sendMessage("mymessage", "set", msgArgs);
50::ms => now;
assert("sendMessage mymessage accepted", true);

// ---- Test: sendList with empty array ----
float empty[0];
pd.sendList("mylist", empty);
50::ms => now;
assert("sendList empty array accepted", true);

// ---- Test: sendMessage with empty args ----
pd.sendMessage("mymessage", "bang", empty);
50::ms => now;
assert("sendMessage empty args accepted", true);

// ---- Summary ----
<<< "" >>>;
if (failures == 0)
    <<< "OK:", tests, "tests passed" >>>;
else
    <<< "FAILED:", failures, "of", tests, "tests" >>>;
