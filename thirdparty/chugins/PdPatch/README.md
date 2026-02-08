# PdPatch Chugin

A ChucK chugin that wraps [libpd](https://github.com/libpd/libpd) to load and run [Pure Data](https://puredata.info/) patches inside ChucK. Route audio through Pd patches, send and receive messages, access Pd arrays, and send MIDI -- all from ChucK code.

## Overview

`PdPatch` extends `UGen` with fixed stereo I/O (2 in, 2 out). Each instance runs its own isolated Pd environment, so multiple patches can coexist independently.

```
PdPatch pd => dac;
pd.open("my-synth.pd", me.dir());
```

### Architecture

- **Block bridging**: Pd processes audio in fixed 64-sample blocks. ChucK may call the tick function with different frame counts. PdPatch maintains internal buffers to bridge between the two, accumulating input samples and draining output samples one block at a time.
- **Latency**: Fixed 64-sample latency (~1.45 ms at 44100 Hz), inherent to Pd's block-based processing.
- **Multi-instance**: Each `PdPatch` object owns a separate `t_pdinstance` via libpd's multi-instance API (`PD_MULTI`). Instances do not share state.
- **Thread safety**: A per-instance mutex guards all libpd calls. Critical sections are short (one 64-sample block or a single message send).

## Build

```bash
make build_pd
```

Or manually with CMake:

```bash
mkdir -p build && cd build
cmake .. -DENABLE_PDPATCH=ON
cmake --build . --config Release
```

Produces `PdPatch.chug` in `chuck_tilde/examples/chugins/`.

libpd is fetched and built from source automatically via CMake FetchContent (static link). No external dependencies required.

### Verify

```bash
make probe-chugins
# Should show: [chugin] PdPatch.chug [OK]
```

### Test

```bash
make test-pdpatch
```

Runs a standalone ChucK test suite (48 assertions) covering patch loading, send/receive messaging, array access, multiple instances, and error handling.

## API Reference

### Class: PdPatch (extends UGen)

Stereo UGen: 2 audio inputs, 2 audio outputs.

```
// Basic usage: patch to dac for output
PdPatch pd => dac;

// Or process audio through a Pd effect
adc => PdPatch fx => dac;
```

---

### Patch Lifecycle

#### `int open(string patchFile, string dir)`

Open a Pd patch file. The patch begins processing audio immediately.

- `patchFile` -- filename of the `.pd` file (e.g. `"synth.pd"`)
- `dir` -- directory containing the patch (e.g. `me.dir()`)
- Returns `0` on success, `-1` on failure.

If a patch is already open, it is closed before opening the new one.

```chuck
pd.open("synth.pd", me.dir()) => int result;
if (result < 0) {
    <<< "Failed to open patch" >>>;
    me.exit();
}
```

#### `void close()`

Close the currently loaded patch. Audio output drops to silence.

```chuck
pd.close();
```

#### `void addSearchPath(string path)`

Add a directory to Pd's search path for abstractions and externals. Call before `open()` if your patch depends on abstractions in other directories.

```chuck
pd.addSearchPath("/path/to/my/pd-abstractions");
```

---

### Send Messages to Pd

These methods send messages to named receivers in the Pd patch. The receiver name corresponds to a `[receive]` (or `[r]`) object in Pd.

#### `void sendBang(string receiver)`

Send a bang.

```chuck
pd.sendBang("trigger");
```

#### `void sendFloat(string receiver, float value)`

Send a float value.

```chuck
pd.sendFloat("freq", 440.0);
pd.sendFloat("gain", 0.5);
```

#### `void sendSymbol(string receiver, string symbol)`

Send a symbol (string).

```chuck
pd.sendSymbol("waveform", "sawtooth");
```

#### `void sendList(string receiver, float[] values)`

Send a list of floats.

```chuck
[100.0, 200.0, 300.0] @=> float partials[];
pd.sendList("partials", partials);
```

#### `void sendMessage(string receiver, string msg, float[] args)`

Send a typed message with a selector and float arguments. This corresponds to Pd's message format like `[set 1 2 3(`.

```chuck
[1.0, 0.5] @=> float args[];
pd.sendMessage("filter", "set", args);
```

---

### Receive Messages from Pd

To receive messages from Pd, first `bind()` to a sender name, then poll for values. The sender name corresponds to a `[send]` (or `[s]`) object in the Pd patch.

The receive model is **last-value**: `getFloat()` and `getSymbol()` return the most recently received value. `getBang()` returns whether a bang was received since the last call (and resets the flag).

All Pd message types are handled: floats, bangs, symbols, lists (first element stored), and typed messages from `[msg ...]` boxes (message selector stored as symbol).

#### `void bind(string receiver)`

Subscribe to messages from a named sender in Pd.

```chuck
pd.bind("output-freq");
pd.bind("status");
```

#### `void unbind(string receiver)`

Unsubscribe from a named sender.

```chuck
pd.unbind("output-freq");
```

#### `float getFloat(string receiver)`

Get the last float received from the named sender. Returns `0.0` if no float has been received or the name is not bound.

```chuck
pd.bind("analyzed-pitch");
// ... later, after Pd has sent values ...
pd.getFloat("analyzed-pitch") => float pitch;
```

#### `string getSymbol(string receiver)`

Get the last symbol received from the named sender. Returns `""` if none received.

```chuck
pd.bind("status");
pd.getSymbol("status") => string s;
```

#### `int getBang(string receiver)`

Returns `1` if a bang was received from the named sender since the last call to `getBang()`, `0` otherwise. The flag is consumed (reset to 0) on each call.

```chuck
pd.bind("onset");
// polling loop
while (true) {
    if (pd.getBang("onset")) {
        <<< "Onset detected!" >>>;
    }
    10::ms => now;
}
```

---

### Array Access

Read and write Pd arrays (tables) from ChucK. The array name corresponds to an `[array]` or `[table]` object in the Pd patch.

#### `int arraySize(string name)`

Get the size of a named Pd array. Returns `-1` if the array does not exist.

```chuck
pd.arraySize("wavetable") => int size;
```

#### `int readArray(string name, float[] dest, int offset)`

Read from a Pd array into a ChucK float array.

- `name` -- Pd array name
- `dest` -- ChucK array to fill (reads `dest.size()` elements)
- `offset` -- starting index in the Pd array
- Returns `0` on success, non-zero on failure.

```chuck
float buf[512];
pd.readArray("wavetable", buf, 0);
```

#### `int writeArray(string name, float[] src, int offset)`

Write a ChucK float array into a Pd array.

- `name` -- Pd array name
- `src` -- ChucK array to write from (writes `src.size()` elements)
- `offset` -- starting index in the Pd array
- Returns `0` on success, non-zero on failure.

```chuck
float waveform[512];
for (0 => int i; i < 512; i++)
    Math.sin(2 * pi * i / 512.0) => waveform[i];
pd.writeArray("wavetable", waveform, 0);
```

---

### MIDI Send

Send MIDI messages to the Pd patch. These are received by Pd objects such as `[notein]`, `[ctlin]`, `[pgmin]`, `[bendin]`, `[touchin]`, and `[polytouchin]`.

Channel numbering follows libpd convention (0-indexed).

#### `void noteOn(int channel, int pitch, int velocity)`

```chuck
pd.noteOn(0, 60, 100);  // channel 0, middle C, velocity 100
```

#### `void noteOff(int channel, int pitch, int velocity)`

Sends a note-on with velocity 0 (standard MIDI note-off).

```chuck
pd.noteOff(0, 60, 0);
```

#### `void controlChange(int channel, int controller, int value)`

```chuck
pd.controlChange(0, 1, 64);  // modwheel to 64
```

#### `void programChange(int channel, int program)`

```chuck
pd.programChange(0, 5);
```

#### `void pitchBend(int channel, int value)`

```chuck
pd.pitchBend(0, 8192);  // center
```

#### `void aftertouch(int channel, int value)`

Channel pressure (mono aftertouch).

```chuck
pd.aftertouch(0, 100);
```

#### `void polyAftertouch(int channel, int pitch, int value)`

Polyphonic aftertouch for a specific note.

```chuck
pd.polyAftertouch(0, 60, 80);
```

---

## Examples

### Basic: Sine Oscillator from Pd

**test-osc.pd** -- A Pd patch with a sine oscillator controlled by `[r freq]`:

```
[loadbang] -> [f 440] -> [s freq]
[r freq]   -> [osc~]  -> [*~ 0.3] -> [dac~]
```

**test-osc.ck** -- ChucK script that loads the patch and sweeps frequencies:

```chuck
PdPatch pd => dac;

pd.open("test-osc.pd", me.dir());

// Play at 440 Hz (default from loadbang)
1::second => now;

// Change to 880 Hz
pd.sendFloat("freq", 880.0);
1::second => now;

// Change to 220 Hz
pd.sendFloat("freq", 220.0);
1::second => now;
```

### Bidirectional: Send and Receive

```chuck
PdPatch pd => dac;
pd.open("analyzer.pd", me.dir());

// Subscribe to Pd's output
pd.bind("rms-out");

// Polling loop
while (true) {
    pd.getFloat("rms-out") => float rms;
    <<< "RMS:", rms >>>;
    100::ms => now;
}
```

### Audio Effect

Route ChucK audio through a Pd reverb patch:

```chuck
SinOsc osc => PdPatch reverb => dac;
440 => osc.freq;

reverb.open("reverb.pd", me.dir());
reverb.sendFloat("wet", 0.3);

5::second => now;
```

### Multiple Independent Instances

Each PdPatch has its own Pd instance -- no shared state:

```chuck
PdPatch synth1 => dac;
PdPatch synth2 => dac;

synth1.open("synth.pd", me.dir());
synth2.open("synth.pd", me.dir());

synth1.sendFloat("freq", 440.0);
synth2.sendFloat("freq", 660.0);  // independent

2::second => now;
```

## Notes

- **Sample rate**: Pd runs at ChucK's sample rate. Patches should use `[samplerate~]` instead of hardcoding 44100 if sample-rate-dependent calculations are needed.
- **Pd print output**: Messages from Pd's `[print]` object are forwarded to stderr with a `[Pd]` prefix.
- **DSP**: DSP is turned on automatically when the PdPatch is constructed. You do not need to send `[; pd dsp 1(` manually.
- **Cleanup**: Patches and receiver bindings are automatically cleaned up when the PdPatch object is destroyed.
- **Typed messages**: Pd `[msg hello]` boxes send through the "anything" handler, not the symbol handler. PdPatch handles both: `getSymbol()` returns the message selector for typed messages and the symbol value for pure symbol messages.
