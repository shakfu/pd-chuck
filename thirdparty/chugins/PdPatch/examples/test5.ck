// test5.ck -- Test for test5.pd (also covers test_full.pd)
//
// test5.pd contains:
//   [adc~ 1] -> [*~ 3] -> [dac~]     (audio passthrough with gain)
//   [r spam] -> [print] + [s eggs]    (message echo: spam -> eggs)
//   array1: 64-element float array    (pre-filled waveform data)
//   [loadbang] -> [f 63] -> [noteout 7]

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
pd.open("test5.pd", me.dir()) => int result;
assert("test5.pd opens", result == 0);
100::ms => now;

// ---- Test: message round-trip (spam -> eggs) ----
pd.bind("eggs");

pd.sendBang("spam");
100::ms => now;
pd.getBang("eggs") => int bangEcho;
assert("spam->eggs bang echo", bangEcho == 1);

// getBang consumed
pd.getBang("eggs") => int bangConsumed;
assert("eggs bang consumed", bangConsumed == 0);

// ---- Test: arraySize ----
pd.arraySize("array1") => int asize;
assert("array1 size is 64", asize == 64);

// ---- Test: readArray ----
float buf[64];
pd.readArray("array1", buf, 0) => int readResult;
assert("readArray returns 0", readResult == 0);

// Verify first element is non-zero (array is pre-filled)
assert("array1[0] is non-zero", buf[0] != 0.0);

// Verify known value from the patch data
// First element in test5.pd: 0.0428571
assert("array1[0] approx 0.0429", buf[0] > 0.042 && buf[0] < 0.044);

// ---- Test: writeArray + readArray round-trip ----
float writeData[4];
1.0 => writeData[0];
2.0 => writeData[1];
3.0 => writeData[2];
4.0 => writeData[3];

pd.writeArray("array1", writeData, 0) => int writeResult;
assert("writeArray returns 0", writeResult == 0);

float readBack[4];
pd.readArray("array1", readBack, 0) => int readResult2;
assert("readArray after write returns 0", readResult2 == 0);
assert("written [0]=1.0 read back", readBack[0] == 1.0);
assert("written [1]=2.0 read back", readBack[1] == 2.0);
assert("written [2]=3.0 read back", readBack[2] == 3.0);
assert("written [3]=4.0 read back", readBack[3] == 4.0);

// ---- Test: arraySize on nonexistent array ----
pd.arraySize("nonexistent_array") => int badSize;
assert("nonexistent array returns -1", badSize == -1);

// ---- Summary ----
<<< "" >>>;
if (failures == 0)
    <<< "OK:", tests, "tests passed" >>>;
else
    <<< "FAILED:", failures, "of", tests, "tests" >>>;
