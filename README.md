# pd-chuck

A project which minimally embeds the [chuck](https://chuck.stanford.edu) engine in a puredata external.

Includes an external, `chuck~` with the following features and limitations:

- Generate and process audio via an embedded chuck engine by running chuck files with global parameters adjusted and controlled in realtime via pd messages.

- Layer sounds by running multiple chuck files (or `shreds`) concurrently.

- Add and remove audio and audio processes on the fly via pd messages.

- Includes [standard chugins](https://github.com/ccrma/chugins) except [`Faust`, `FluidSynth`, `Ladspa`, `MIAP`]

- As of this version, there is no support for callbacks and events except via the `signal` and `broadcast` messages.

see `chuck~/help-chuck.pd` for a basic demo of current features.


Note that this is project is the sibling to [chuck-max](https://github.com/shakfu/chuck-max), a Max-MSP external with similar features.

The current chuck version used is `1.5.0.8`


## Status

Currently producing audio in a minimal proof-of-concept kind of way.

- [ ] No Windows support yet
- [ ] No Linux support yet
- [ ] Add multichannel support
- [ ] Add rest of the standard chugins
- [ ] Add support for callbacks and events
- [x] Audio quality needs improvement (fix thanks to Professor GE Wang!)
- [x] No errors during compilation of external
- [x] Instantiate Chuck class without errors.
- [x] Create demo with audio.


## Building

Only tested on macOS and needs `cmake` to build.

Easy way:

```bash
git clone https://github.com/shakfu/pd-chuck
cd pd-chuck
make
```

Or, long way:

```bash
git clone https://github.com/shakfu/pd-chuck
cd pd-chuck
mkdir build && cd build
cmake ..
make
```

Then open `chuck~/help-chuck.pd` for a basic demo.
