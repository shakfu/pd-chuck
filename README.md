# pd-chuck

A project which minimally embeds the [chuck](https://chuck.stanford.edu) engine in a puredata external.

Includes an external, `chuck~` with the following features and limitations:

- Generate and process audio via an embedded chuck engine running chuck files with global parameters adjusted and controlled in realtime via pd messages.

- Layer sounds by running multiple chuck files (or `shreds`) concurrently.

- Add and remove audio and audio processes on the fly via pd messages.

- Includes [standard chugins](https://github.com/ccrma/chugins) except [`Faust`, `FluidSynth`, `Ladspa`, `MIAP`]

- As of this version, there is no support for callbacks and events except via the `signal` and `broadcast` messages.

see `chuck~/help-chuck.pd` for a basic demo of current features.

Note that this is project is the sibling to [chuck-max](https://github.com/shakfu/chuck-max), a Max-MSP external with similar features.

The current chuck version used is `1.5.2.2-dev (chai)`


## Status

- [ ] Add support for callbacks and events
- [X] Preliminary Linux support
- [ ] No Windows support yet
- [ ] Add multichannel support
- [ ] Add rest of the standard chugins
- [x] Audio quality needs improvement (fix thanks to Professor GE Wang!)
- [x] No errors during compilation of external
- [x] Instantiate Chuck class without errors.
- [x] Create demo with audio.


## Building

Only tested on macOS and linux and needs `cmake` to build.

For macOS requires:

```bash
brew install libsndfile
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

**Or, long way**:

```bash
git clone https://github.com/shakfu/pd-chuck
cd pd-chuck
mkdir build && cd build
cmake ..
cmake --build . --config Release
```

For linux you can also use cmake linux boolean options, `LINUX_ALSA`, `LINUX_PULSE`, `LINUX_JACK` and `LINUX_ALL` which correspond the linux audio driver options. (Look at `pd-chuck/Makefile` for further details.)

Then open `chuck~/help-chuck.pd` for a basic demo.
