# Syncing a Pd transport with ChucK

How to keep ChucK in time with a transport built in a Pd patch: a `[metro]`
sequencer, a tempo/play/seek controller, or a `[phasor~]`.

## Why it never drifts

`ck_perform` calls `ChucK::run()` once per Pd DSP block, for exactly that
block's samples. ChucK's `now` and Pd's logical time therefore advance from
one sample count. A tempo sent once stays locked; nothing needs resending to
correct drift. The only question is how precisely each change lands.

## Three approaches

| Approach | Pd sends | ChucK does | Precision | Cost |
|-|-|-|-|-|
| A. Events | `msg step <n>` per step | waits on a `HostIn` | each event rounded to a block, up to 64 samples (1.45 ms at 44.1 kHz) | none |
| B. State | `msg clock tempo 120`, `play`, `seek`, `sig`, on change only | schedules beats from the tempo with `now` | changes rounded to a block; beats between them sample-exact | `HostClock.ck` |
| C. Signal phase | a `[phasor~]` into a `chuck~` signal inlet | detects the wrap per sample | sample-exact | one shred wakes per sample |

Choose by the shape of the transport:

- B for a steady tempo with start, stop and seek. Pd only reports
  changes; ChucK places every beat itself.
- C when Pd's position must drive ChucK to the sample.
- A for irregular events Pd decides one at a time: swing, step sequencers.

### A. Events

```chuck
HostIn pin;
pin.addAddress( "step" );
HostMsg m;
while( true ) {
    pin => now;
    while( pin.recv( m ) ) play( m.getInt( 0 ) );
}
```

Pd: `[msg step 3(` -> `[chuck~]`. `HostIn` wakes shreds at the first sample
of the next block, so each event lands on a block boundary. Pd fires clocks
between DSP ticks, so an event may land up to one block early relative to its
Pd logical time (inferred from `sched_tick`, not measured).

### B. State: `HostClock.ck`

`chuck_tilde/examples/pd/HostClock.ck` is a ChucK class with the API of
chuck-max's built-in `HostClock`. It keeps the transport in ChucK and takes
changes from the patch:

| Pd -> `[chuck~]` | Effect |
|-|-|
| `msg clock tempo <bpm>` | tempo; `bpm` > 0 |
| `msg clock play <0\|1>` | stop / start |
| `msg clock seek <ticks>` | position, 480 ticks per quarter note |
| `msg clock sig <num> <denom>` | time signature |

```chuck
@import "HostClock.ck"
HostClock clk;
while( true ) {
    clk.toNextBeat() => now;     // never zero
    <<< clk.bar(), clk.beatInBar() >>>;
}
```

`examples/pd/pd_clock.ck` runs chuck-max's `max_clock.ck` loop unchanged.

API, as in chuck-max: `ticks()`, `tempo()`, `running()`, `beat()`,
`toNextBeat()`, `toNext(ticks)`, `bar()`, `beatInBar()`, `unit()`. A
`HostClock` is an `Event` and broadcasts once per block that changed it.

Behaviour:

- Defaults before any message: 120 BPM, 4/4, stopped at tick 0, as Max's
  transport starts.
- A change applies at the block where the message arrives. Position is
  continuous across `tempo`, `play` and `sig`.
- Repeating the current value is not a change and does not broadcast. A
  `seek` within half a sample of the current position is ignored, as in Max.
- Invalid messages are rejected with a warning (`HostClock: tempo must be
  > 0`). Every `HostClock` posts it, so two clocks post two copies.
- Every `HostClock` shares the last state through static members, so a
  clock created later starts in sync. Each one listens with its own `HostIn`.
- A clock stops following the patch when the shred that created it ends, as
  a `global HostIn` does (`bind.md`).

Differences from chuck-max:

- Max reads its transport every block. Here the patch sends changes only.
- `@import "HostClock.ck"` compiles only in pd-chuck. chuck-max binds its own
  `HostClock`, and a second definition is a compile error. Code that uses only
  `HostClock clk;` runs in both.
- Resolution is fixed at 480 ticks per quarter note.

`make test-bind` runs `tests/test_bind_clock.pd` with
`examples/test/bind_clock.ck`. It covers 20 checks: defaults, start, beat
boundaries, `toNext` never zero, seek, time signature, continuity across a
tempo change, a late clock, stop, rejected messages and the broadcast count.

### C. Signal phase

```chuck
adc.chan( 0 ) => blackhole;       // [phasor~] into chuck~'s left inlet
Event beat;
0.0 => float last;
while( true ) {
    adc.chan( 0 ).last() => float ph;
    if( ph < last ) beat.broadcast();   // wrapped: a new beat
    ph => last;
    1::samp => now;
}
```

This uses one `chuck~` input channel. Not tested.

## Inverting it: ChucK as master

ChucK schedules beats to the sample. `Host.sendFloat( "beat", n )` reaches
`[r beat]` in the same logical time, after the DSP tick (`bind.md`). Pd
messages land on block boundaries anyway, so making ChucK the transport and
Pd the follower is often simpler.

## Why not a bound `HostClock` in C++

Pd has no global transport for C++ to read. A bound class could only store
what the patch sends, which `HostClock.ck` already does without native code.
A C++ class is worth writing only for a source that produces a snapshot every
block, for example `[abl_link~]`. For Ableton Link, the AbletonLink chugin
already syncs ChucK directly.
