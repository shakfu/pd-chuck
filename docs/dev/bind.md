# Host-defined ChucK classes via `ChucK::bind()`

`ck_new` binds five classes into each VM after `ChucK::init()`:

| Direction | Class | Delivery |
|-|-|-|
| ChucK to Pd | `Host.msg()` | reply queue, next drain (20 ms poll) |
| ChucK to Pd | `Host.sendBang/Float/Symbol/List()` | `[r <receiver>]`, after the current DSP tick |
| Pd to ChucK | `HostIn` / `HostMsg` | next block, at its first sample |
| ChucK reads/writes Pd | `HostBuffer` | immediate |
| Array playback | `HostBufPlay` UGen | per sample |

Ported from chuck-max (`source/docs/dev/bind.md` there). `HostClock` and the
`clockprobe` prototype are not bound: Pd has no global transport.
`examples/pd/HostClock.ck` is a ChucK class with the same API, driven by
the patch; see `syncing.md`.

The class names are host-neutral and shared with chuck-max and numchuck, so
one `.ck` file can run in each host that binds them. They were `Pd`, `PdIn`,
`PdMsg`, `PdArray` and `PdArrayPlay` until 2026-10-24.

## Host support

A `.ck` file runs unchanged in each host that binds every member it uses. A
missing class or member is a compile error. State on 2026-10-27, from each
repo's query functions:

| Member | pd-chuck | chuck-max | numchuck |
|-|-|-|-|
| `Host.msg` | yes | yes | yes |
| `Host.sendBang/Float/Symbol/List` | yes | yes | yes |
| `HostIn`, `HostMsg` | yes | yes | no |
| `HostBuffer`, `HostBufPlay` | yes, mono | yes | yes |
| `HostClock` | `HostClock.ck` (`syncing.md`) | yes | no |

Types go into the `global` namespace (`Chuck_Compiler::bind`), so they
survive `reset` (`CK_MSG_CLEARVM` clears only the user namespace).

## Threading

Pd runs the perform routine and message methods under one lock. In callback
mode `sched_audio_callbackfn` takes `sys_lock()` and then calls
`sched_tick()`, which runs clocks, messages and DSP (`m_sched.c`, Pd 0.57).
ChucK code therefore never runs concurrently with a message method. Pd's own
externals rely on this: `tabwrite~` calls `garray_redraw` from its perform
routine (`d_array.c`).

## `Host.msg`

```chuck
Host.msg("freq", 440.0);          // reply outlet: msg freq 440
Host.msg("go");                   // msg go
Host.msg("file", "kick.wav");     // msg file kick.wav
Host.msg("xy", [0.2, 0.7]);       // msg xy 0.2 0.7
```

Pd: `[chuck~]` rightmost outlet -> `[route msg]` -> `[route freq]`.

`Host.msg` goes to the reply outlet; `Host.send*` goes to `[r <receiver>]`.
It was `Host.send(name, value)` until 2026-10-27. `Host.msg` out mirrors
`HostIn` receiving `msg <name> <args>`; `send*` keeps the `[s]`/`[r]` idiom.

- Needs `reply 1`. Without it, `ck_reply_push` drops the message.
- Four overloads: no value, `float`, `string`, `float[]`. A list keeps at
  most 63 values (`CK_REPLY_MAX_ATOMS - 1`), cut silently; an empty list
  sends `msg <name>`.
- The selector is always `msg`; the name is an argument. No ChucK name can
  collide with the reply vocabulary (`val`, `event`, `shred`, `global`).
- A full reply queue (256) drops the message silently.

## `Host.sendBang` / `sendFloat` / `sendSymbol` / `sendList`

```chuck
Host.sendBang("go");                  // every [r go] gets bang
Host.sendFloat("freq", 440.0);        // ... 440
Host.sendSymbol("file", "kick.wav");  // ... symbol kick.wav
Host.sendList("xy", [0.2, 0.7]);      // ... list 0.2 0.7
```

`Host.msg` matches chuck-max and numchuck. These four are the
Pd-native route; names and signatures match the PdPatch chugin.

- Works without `reply 1`.
- Queued in the perform routine; a 0 ms clock calls `pd_float` on the
  receiver's `s_thing`. A direct call could edit the DSP graph during its
  traversal. `clock_delay` from perform is standard: `env~` does it
  (`d_ctl.c`). Latency: same logical time, after the DSP tick, versus up to
  20 ms for `Host.msg`.
- A receiver that does not exist at delivery is skipped, as `[s]` skips it.
- 256 calls, of all four kinds, fit between ticks. Further calls are
  dropped, and the drain posts one `Host.send: queue full, <n> messages
  dropped` warning.
- `sendList` keeps 32 elements and warns `<n> lists cut to 32 elements`.
  An empty list arrives as bang, as it does through `[s]`.
- Receive names are shared by the whole Pd process. Two `chuck~` objects, or
  two copies of an abstraction, that call `sendFloat("freq", ...)` reach the
  same `[r freq]`. `Host.msg` goes to the sending object's outlet only. Give
  each copy its own receiver name (`$0-freq` style) when they must not
  collide.
- Timing: delivery is at the end of the DSP tick that queued the call, so it
  is block-quantized (64 samples) and never sample-accurate. Unlike Max,
  there is no thread jitter: Pd runs clocks and DSP in one thread.
- Lists hold floats only. ChucK arrays are homogeneous, so a mixed list such
  as `["note", 60]` has no ChucK type, and the queue entry is a fixed array
  of 32 floats. Send a string with `sendSymbol`. A `string[]` overload of
  `sendList` is the next step if one host needs it.

## `HostIn` / `HostMsg`

```chuck
HostIn pin;
pin.addAddress("note");
HostMsg m;
while (true) {
    pin => now;                  // HostIn extends Event
    while (pin.recv(m)) {
        m.getInt(0) => int pitch;
        m.getFloat(1) => float vel;
        m.getString(2) => string art;
    }
}
```

Pd: `[msg note 60 0.8 legato(` -> `[chuck~]`.

- `ck_msg` calls `pdin_deliver` directly. The chuck-max version queues in a
  ring and drains it before `run()`. Pd's lock makes the ring unnecessary.
  Timing is the same: shreds wake at the first sample of the next block.
- Pd numbers are floats. `getInt` truncates, so `msg note 61.7` reads 61.
- Missing arguments and type mismatches read 0 or `""`.
- Limits: 32 arguments (`ck_msg` rejects more); 64 unread messages per
  `HostIn`, further messages are dropped.
- A message with no listener is dropped, not held.
- `msg` is now an inlet selector. A ChucK global named `msg` must be set
  with `set`.
- `verbose 2` logs `msg <name>: delivered to <n> HostIn`.

Lifecycle, unchanged from chuck-max:

- The dtor unregisters a `HostIn` freed while its shred runs.
- A removed shred never releases its locals, so their dtors never run.
  `pdin_watcher` (`ckvm_shreds_watch_REMOVE`) unregisters every `HostIn` the
  removed shred created. This covers `removeall`, `remove` and `reset`.
- A `global HostIn` stops receiving when its creating shred ends.

## `HostBuffer` / `HostBufPlay`

```chuck
HostBuffer a;
a.name("ckbuf");          // ready() is 1 at once if the array exists
a.peek(0) => float x;
a.poke(100, 0.5);

HostBufPlay p => dac;
p.name("ckbuf");          // restarts from point 0
1 => p.loop;
-0.5 => p.rate;           // points per sample; negative plays backward
```

- `HostBuffer`: `name`, `ready`, `frames`, `channels`, `sampleRate`,
  `peek(i)`, `peek(i, chan)`, `poke(i, v)`, `poke(i, chan, v)`.
- `HostBufPlay`: `name`, `ready`, `frames`, `pos`, `rate`, `loop`
  (default 0), `interp` (1 linear, the default; 0 none), `channel`.
- Mono. A Pd array is one channel: `channels()` is 1 when ready, and any
  channel other than 0 reads 0, ignores writes and plays silence. Setting
  `HostBufPlay.channel` to another channel posts `HostBufPlay: channel <n>
  plays silence` once per object; stereo code written for Max would
  otherwise go silent with no message.
- `sampleRate()` is the patch rate, so `HostBufPlay` never resamples. A Pd
  array does not record the rate of the file loaded into it, and
  `soundfiler` does not resample: a 44.1 kHz file in a 48 kHz patch plays
  about 9% sharp while `sampleRate()` reports 48000. Returning 0 would make
  the Max idiom of dividing by `sampleRate()` produce inf. Pass the file
  rate from the patch (`soundfiler` reports it) if pitch matters.

Implementation (`pdarray.cpp`):

- Each instance has 64 name slots, shared by both classes. Slots outlive
  shreds and `reset`.
- `pdarray_refresh` runs before `run()` and re-reads every slot with
  `pd_findbyclass` and `garray_getfloatwords`. Pd resizes and frees arrays
  only between blocks, so the pointers hold for the block. By construction,
  a resized, deleted or re-created array is followed; only resize is tested.
- Caching the pointer across blocks was rejected. Pd's `d_array.c`
  validates a cached pointer with `gpointer_setarray`, declared in
  `g_canvas.h`, not `m_pd.h`. The vendored `m_pd.h` is 0.53, and older Pd
  may not export it (not checked).
- `poke` marks the slot dirty. `pdarray_flush` runs after `run()` and calls
  `garray_redraw` once per dirty slot.
- Two arrays with the same name make `pd_findbyclass` post
  `warning: <name>: multiply defined` once per block while a slot uses it.

## Testing

`make test-bind`, part of `make test`, runs Pd headless (`pd -batch`):

- `chuck_tilde/tests/test_bind.pd` runs `examples/test/bind_test.ck`: 28
  checks over `Host.msg` (the four overloads), `HostBuffer` (range, sharing, missing array, resize,
  the channel and sample-rate members),
  exact `HostBufPlay` output (one-shot, looped half-rate interpolation,
  backward), and `HostIn` (typed arguments, truncation, mismatches, address
  filtering, two receivers, order).
- `chuck_tilde/tests/test_bind_lifecycle.pd` counts receivers per message
  across `removeall`, a rerun, `reset` and a rerun: `2 0 2 0 2`.
- `chuck_tilde/tests/test_bind_send.pd` runs the four `[r]` senders without
  `reply 1`: each type arrives intact, a missing receiver is skipped, empty
  and 40-element lists arrive as bang and 32 elements, and 300 calls in one
  block deliver 256 and warn about 44.
- `chuck_tilde/tests/test_bind_clock.pd` drives `HostClock.ck`; see
  `syncing.md`.

Not tested: `garray_redraw` (no GUI in batch mode), the duplicate-name
warning.
