# pd-chuck

A project which minimally embeds the [chuck](https://chuck.stanford.edu) engine in a puredata external.

## chuck~

Includes an external, `chuck~` with the following features and limitations:

- Generate and process audio via an embedded chuck engine running chuck files with global parameters adjusted and controlled in realtime via pd messages.

- Layer sounds by running multiple chuck files (or `shreds`) concurrently.

- Add and remove audio and audio processes on the fly via pd messages.

- Includes [standard base chugins](https://github.com/ccrma/chugins) including `WarpBuf`, `Faust` except [`FluidSynth`, `Ladspa`]

- Includes support for callbacks and events via the `signal` and `broadcast` messages.

- Includes support for core chuck messages (`add`, `remove`, `replace`, `status`, `clear vm`, etc..) via pd messages.

see `chuck~/help-chuck.pd` for a basic demo of current features.

Note that this is project is the sibling to [chuck-max](https://github.com/shakfu/chuck-max), a Max-MSP external with similar features.

The current chuck version used is `1.5.2.3-dev (chai)`


## faustgen2~

In addition, another pure-data external, [faustgen2~] by Albert Graef, is included for macOS for no other reason than the dependencies, due to the `Faust.chugin`, are already available and it would be nice to have two faust-related externals for the price of one.

Builds with `make faustgen`


## Status

- [ ] No Windows support yet
- [x] Add support for callbacks and events
- [X] Preliminary Linux support
- [x] Add rest of the standard chugins
- [x] Audio quality needs improvement (fix thanks to Professor GE Wang!)
- [x] No errors during compilation of external
- [x] Instantiate Chuck class without errors.
- [x] Create demo with audio.


## Building

Only tested on macOS and linux and needs `cmake` to build.

To build everything use:

```
make full
```

to build a smaller set of dependencies sue:

```
make light
```

For linux, the dependencies are more varied and follow the chuck linux build options. To install all linux dependencies:

```bash
sudo apt install build-essential bison flex libsndfile1-dev \
  libasound2-dev libpulse-dev libjack-jackd2-dev
```

**Easy way**:

```bash
git clone https://github.com/shakfu/pd-chuck
cd pd-chuck
make
```

On linux this will build the default alsa option (which can also be built via `make linux-alsa`.

If you need jack or pulse audio support, use `make linux-jack` or `make linux-pulse`, or if you want to build with all supported linux drivers, use `make linux-all`

On macOS, if you want to build for a particular macos deployment target:

```bash
make MACOSX_DEPLOYMENT_TARGET=12.6
```

**Or, long way** if you already have depdencies installed:

```bash
git clone https://github.com/shakfu/pd-chuck
cd pd-chuck
mkdir build && cd build
cmake ..
cmake --build . --config Release
```

For linux you can also use cmake linux boolean options, `LINUX_ALSA`, `LINUX_PULSE`, `LINUX_JACK` and `LINUX_ALL` which correspond the linux audio driver options. (Look at `pd-chuck/Makefile` for further details.)

Then open `chuck~/help-chuck.pd` for a basic demo.
