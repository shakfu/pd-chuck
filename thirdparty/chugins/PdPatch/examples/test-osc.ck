// test-osc.ck -- Load a Pd patch that generates a sine wave
// The Pd patch (test-osc.pd) contains an [osc~] driven by [r freq]

PdPatch pd => dac;

// Open the patch (filename, directory)
pd.open("test-osc.pd", me.dir()) => int result;
if (result < 0)
{
    <<< "ERROR: failed to open test-osc.pd" >>>;
    me.exit();
}

<<< "Pd patch loaded, playing 440 Hz sine..." >>>;

// Let it play for 1 second at default 440 Hz
1::second => now;

// Change frequency via Pd's message system
<<< "Changing frequency to 880 Hz..." >>>;
pd.sendFloat("freq", 880.0);

// Play for another second
1::second => now;

// Change again
<<< "Changing frequency to 220 Hz..." >>>;
pd.sendFloat("freq", 220.0);

1::second => now;

<<< "Done." >>>;
