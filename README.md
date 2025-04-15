# pd-chuck

Embeds the [ChucK](https://chuck.stanford.edu) audio engine in a puredata external.

The `chuck~` external works on MacOS and Linux and uses a bleeding edge ChucK version: `1.5.5.1-dev (chai)`. It has the following features:

- Generate and process audio by running chuck files and evaluating chuck code with `global` parameters controlled and adjusted in realtime by pd messages.

- Layer sounds from a single instance by running multiple chuck files concurrently.

- Add, remove, replace audio and audio processes on the fly using chuck messages via pd messages.

- Interact with pd via MIDI and OSC protocols.

- Includes support for callbacks and events via the `signal` and `broadcast` messages.

see `chuck_tilde/help-chuck.pd` for a basic overview of current features, and `chuck_tilde/test` for feature-specific test patches.

Also included are the following:

- The complete set of current chuck examples

- All of the [CCRMA chugins](https://github.com/ccrma/chugins) including `WarpBuf`, `Fauck` (`Faust`) and `FluidSynth`.

- Many `pd` patches to test and demonstrate usage.

- Contributed patchers and code examples.

*For the impatient*: download the `pd-chuck` package with pre-compiled externals and chugins from the the project's [Releases](https://github.com/shakfu/pd-chuck/releases) section and check out the [cheatsheat](https://github.com/shakfu/chuck-max/blob/main/media/chuck-max-cheatsheat.pdf).

Note that `pd-chuck` has a sibling in the [chuck-max](https://github.com/shakfu/chuck-max) project.

## Usage

The `pd-chuck` package consists of the following folders:

```bash
pd-chuck
├── chuck_tilde
│  ├── examples
│  │  ├── ai
│  │  ├── ...
│  │  ├── chugins
│  │  ├── ...
│  │  ├── data
│  │  ├── ...
│  │  ├── fauck
│  │  ├── faust
│  │  ├── ...
│  │  ├── pd
│  │  ├── ...
│  │  ├── test
│  │  ├── ...
│  │  └── warpbuf
│  └── tests
├── docs
├── scripts
└── thirdparty
    ├── chuck
    └── chugins
```

Start with the `help-chuck.pd` file in the `chuck_tilde` folder for an overview of the external's features.

The `examples` directory in the same folder contains all chuck examples from the chuck repo, and some additional folders: `chugins` containing chugin binaries, `fauck`, containing `faust.chug` examples, `faust`, containing the faust stdlib, `pd` chuck files which are used by pd patches. There are also quite a few patches demonstrating one feature or other in the `tests` folder,

## Overview

- The `chuck~` object has two signal channels in and two channels out by default. It can take an optional filename argument `[chuck~ <filename>]`.
  
If a `<filename>` argument is given it will be searched for according to the following rules:

1. Assume it's an absolute path, use it if it exists.

2. Assume that it's a partial path with the package's `examples` folder as a prefix. So if `stk/flute.ck` is given as `<filename>`, the absolute path of the package `examples` folder is prepended to it and if the resulting path exists, it is used.

3. Assume `<filename>` exists in the parent patcher's directory. Test that this is the case and if it is, use it. This is useful if you want to package patchers and chuck files together.

### Core Messages

As of the current version, `chuck~` implements the core Chuck vm messages as puredata messages:

| Action                            | Puredata msg                 | Puredata msg (alias)         |
| :-------------------------------- | :--------------------------- | :--------------------------  |
| Add shred from file               | `add <file> [arg1 arg2 .. ]` | `+ <filepath> [args]`        |
| Run chuck file (save last used)   | `run <file>`                 |                              |
| Eval code as shred                | `eval <code>`                |                              |
| Remove shred                      | `remove <shredID>`           | `- <shredID>`                |
| Remove last shred                 | `remove last`                |                              |
| Remove all shreds                 | `remove all`                 |                              |
| Replace shred                     | `replace <shredID> <file>`   | `= <shredID> <file>`         |
| List running shreds               | `status`                     |                              |
| Clear vm                          | `clear vm`                   | `reset`                      |
| Clear globals                     | `clear globals`              |                              |
| Reset id                          | `reset id`                   |                              |
| Time                              | `time`                       |                              |

It's worth reading the [ChucK Language Specification's section on Concurrency and Shreds](https://chuck.cs.princeton.edu/doc/language/spork.html) to get a sense of what the above means. The first paragraph will be quoted here since it's quite informative:

> ChucK is able to run many processes concurrently (the process behave as if they are running in parallel). A ChucKian process is called a shred. To spork a shred means creating and adding a new process to the virtual machine. Shreds may be sporked from a variety of places, and may themselves spork new shreds.

### Utility Messages

The core set of chuck vm messesages is also extended in `pd-chuck` with the following utility messages:

| Action                                  | Puredata msg                 |
| :-------------------------------------- | :--------------------------- |
| Set file attribute (does not run)       | `file <path>`                |
| Set full path to editor attribute       | `editor <path>`              |
| Prevent running shreds when dsp is off  | `run_needs_audio`            |
| Open file in external editor            | `edit <path>`                |
| Probe chugins                           | `chugins`                    |
| Get/set loglevel (0-10)                 | `loglevel` & `loglevel <n>`  |
| Get state of chuck vm                   | `vm`                         |
| Launch chuck docs in a browser          | `docs`                       |

### Parameter Messages

Once a shred is running you can change its parameters by sending values from puredata to the `chuck~` object. To this end, ChucK makes available three mechanisms: global variables, global events, and callbacks which are triggered by events. `chuck~` maps these chuck language elements to corresponding puredata constructs as per the following table:

| Action                            | ChucK              | Puredata msg                 |
| :-------------------------------- | :----------------  | :--------------------------  |
| Change param value (untyped)      | global variable    | `<name>` `<value>`           |
| Dump global variables to console  | global variable    | `globals`                    |
| Trigger named event               | global event       | `sig <name>`                 |
| Trigger named event all shreds    | global event       | `broadcast <name>`           |

You change a global variable by sending a `<variable-name> <value>` message to a `chuck~` instance where the `value` can be an `int`, `float`, `string`, `array of ints` or `floats`, etc. You can also trigger events by sending `sig` or signal messages, `broadcast` messages as per the above table.

*Note*: You can't use the ChucK types of `dur` or `time` in pd. Also, while in the above case, the pd msg seems untyped, it must match the type of the chuck global variable. So if you connect a pd number or flownum object to a message box, it needs to match the type of the global variable (int/float).

See `chuck_tilde/help-chuck.pd` and patchers in the `tests` directory for a demonstration of current features.

### Parameter Messages using Callbacks (Advanced Usage)

In addition to the typical way of changing parameters there is also an extensive callback system which includes listening / stop-listening for events associated with callbacks, triggering them via `sig` and `broadcast` messages and also setting typed global variables via messages and symmetrically getting their values via typed callbacks:

| Action                            | ChucK              | Puredata msg                         |
| :-------------------------------- | :----------------  | :----------------------------------- |
| Listen to event (one shot)        | global event       | `listen <name>` or `listen <name> 0` |
| Listen to event (forever)         | global event       | `listen <name> 1`                    |
| Stop listening to event           | global event       | `unlisten <name>`                    |
| Trigger named callback            | global event       | `sig <name>`                         |
| Trigger named callback all shreds | global event       | `broadcast <name>`                   |
| Get int variable                  | global variable    | `get int <name>`                     |
| Get float variable                | global variable    | `get float <name>`                   |
| Get string variable               | global variable    | `get string <name>`                  |
| Get int array                     | global variable    | `get int[] <name>`                   |
| Get float array                   | global variable    | `get float[] <name>`                 |
| Get int array indexed value       | global variable    | `get int[i] <name> <index>`          |
| Get float array indexed value     | global variable    | `get float[i] <name> <index>`        |
| Get int associative array value   | global variable    | `get int[k] <name> <key>`            |
| Get float associative array value | global variable    | `get float[k] <name> <key>`          |
| Set int variable                  | global variable    | `set int <name> <value>`             |
| Set float variable                | global variable    | `set float <name> <value>`           |
| Set string variable               | global variable    | `set string <name> <value>`          |
| Set int array                     | global variable    | `set int[] <name> v1, v2, ..`        |
| Set float array                   | global variable    | `set float[] <name> v1, v2, ..`      |
| Set int array indexed value       | global variable    | `set int[i] <name> <index> <value>`  |
| Set float array indexed value     | global variable    | `set float[i] <name> <index> <value>`|
| Set int associative array value   | global variable    | `set int[k] <name> <key> <value>`    |
| Set float associative array value | global variable    | `set float[k] <name> <key> <value>`  |

## Building

pd-chuck is currently developed and tested on macOS and Linux. For both of these platforms, it provides a make-based frontend and uses `cmake` as the backend build system.

This project provides for building two general variants depending on the need of end-user:

1. The *base system* consists of the `chuck~` external and the base CCRMA chugins. This is already more than sufficient for more than 80% of users. The base system is relatively easy to build and only needs a c++ compiler, `cmake`, `make`, `bison` and `flex`.

2. The *advanced system* consists of the base system above plus three chugins which are relatively more challenging to build and use: `Faust.chug`, `WarpBuf.chug` and `FluidSynth.chug`.

On macOS requirements for the variants can be installed using [Homebrew](https://brew.sh) as follows:

```bash
brew install cmake bison flex fluidsynth automake autogen libtool rubberband libsamplerate
```

On Debian Linux, requirements can be installed via:

```bash
sudo apt install build-essential cmake bison flex libsndfile1-dev libasound2-dev libpulse-dev libjack-jackd2-dev libmpg123-dev libmp3lame-dev libresample1-dev librubberband-dev libfluidsynth-dev
```

Each build option has a Makefile target as follows:

| Makefile target        | alias        | faust | warpbuf | fluidsynth | .wav   | .mp3    | .others |
| :--------------------- | :----------- | :---: | :-----: | :--------: | :----: | :-----: | :-----: |
| `full`                 |              | x     | x       |            | x      | x       | x       |
| `nomp3`                |              | x     | x       |            | x      |         | x       |
| `light`                |              | x     | x       |            | x      |         |         |
| `macos-base-native`    | `make`       |       |         |            | x      |         |         |
| `macos-base-universal` |              |       |         |            | x      |         |         |
| `macos-adv-brew`       | `make macos` | x     | x       | x          | x      | x       | x       |
| `linux-base-alsa`      | `make`       |       |         |            | x      |         |         |
| `linux-base-pulse`     |              |       |         |            | x      |         |         |
| `linux-base-jack`      |              |       |         |            | x      |         |         |
| `linux-base-all`       |              |       |         |            | x      |         |         |
| `linux-adv-alsa`       | `make linux` | x     | x       |            | x      | x       | x       |
| `linux-adv-pulse`      |              | x     | x       |            | x      | x       | x       |
| `linux-adv-jack`       |              | x     | x       |            | x      | x       | x       |
| `linux-adv-all`        |              | x     | x       |            | x      | x       | x       |

Note: *.others* includes support for .flac, .ogg, .opus, and .vorbis formats

To get a sense of build times, the following table (for macOS builds on an M1 Macbook Air) should be illustrative:

| build command                | build time (secs)  |
| :--------------------------- | :----------------- |
| `make macos-base-native`     | 55.074             |
| `make macos-adv-brew`        | 1:29.24            |
| `make macos-base-universal`  | 1:44.31            |
| `make macos-adv-light`       | 2:27.28            |
| `make macos-adv-nomp3`       | 2:50.15            |
| `make macos-adv-full`        | 3:14.91            |

If you'd rather use the more verbose `cmake` build instructions directly, feel free to look at the `Makefile` which is quite minimal.

### A. The Base System

The base `chuck-max` system consists of a puredata package with the `chuck~` external, the base [CCRMA chugins](https://github.com/ccrma/chugins) and extensive examples, tests and puredata patchers.

If you have installed the prerequisites above, to get up and running on either system with the default build option:

```bash
git clone https://github.com/shakfu/pd-chuck.git
cd pd-chuck
make
```

The complete list of base build options are:

- `make` or `make macos-base-native`: builds the external using your machine's native architecture which is `arm64` for Apple Silicon Macs and `x86_64` for Intel Macs. This is the default build option on macOS.

- `make macos-base-universal`: build the external as a 'universal' binary making it compatible with both Mac architectural variants. This is useful if you want share the external with others in a custom pd package.

- `make` or `make linux-base-alsa`: builds the external on Linux using the default ALSA audio driver. This is the default build base option for Linux.

- `make linux-base-pulse`: builds the external on Linux using the PULSE audio driver.

- `make linux-base-jack`: builds the external on Linux using the JACK audio driver.

- `make linux-base-all`: builds the external on Linux with support for ALSA, PULSE and JACK audio drivers.

### B. The Advanced System

The advanced system consists of the base system + two advanced chugins, `Faust.chug` and `WarpBuf.chug`:

1. The [Fauck](https://github.com/ccrma/fauck) chugin contains the full llvm-based [faust](https://faust.grame.fr) engine and dsp platform which makes it quite powerful and also quite large compared to other chugins (at around 45 MB stripped down). It requires at least 2 output channels to work properly. It also uses the [libsndfile](https://github.com/libsndfile/libsndfile) library.

2. The [WarpBuf](https://github.com/ccrma/chugins/tree/main/WarpBuf) chugin makes it possible to time-stretch and independently transpose the pitch of an audio file. It uses the [rubberband](https://github.com/breakfastquay/rubberband), [libsndfile](https://github.com/libsndfile/libsndfile), and [libsamplerate](https://github.com/libsndfile/libsamplerate) libraries.

3. The [FluidSynth](https://www.fluidsynth.org) chugin, enables a real-time software synthesizer based on the SoundFont 2 specifications, (see above for requirements).

If you have installed the prerequisites above, it should be possible the advanced system with one of the following options:

- `make macos-adv-brew` or `make macos`: build the external using the previously installed homebrew dependencies, as well as downloaded `faust` headers and a downloaded pre-compiled `libfaust` (`libfaustwithllvm`) library. This is the newer, faster, recommended way of getting a full chuck-max system up and running.

- `make macos-adv-full`: build the external by manually downloading and building all of the dependencies except for `libfaust` from source. This is the previous way of building an advanced system. It is currently only for advanced developers who want maximum flexibility in their builds.

- `make macos-adv-nomp3`: same as `make full` except without support for the .mp3 format. Try this variant if you are unable to build using `make full` on Intel Macs or on older macOS versions.

- `make macos-adv-light`: Same as `make full` except for withouth `libsndfile` multi-file format support. This means that (.mp3, flac, vorbis, opus, ogg) formats are not supported in this build. Only `.wav` files can be used.

- `make linux-adv-alsa` or `make linux`: builds the external on Linux using the default ALSA audio driver. This is the default advanced option for Linux.

- `make linux-adv-pulse`: builds the external on Linux with advanced options using the PULSE audio driver.

- `make linux-adv-jack`: builds the external on Linux with advanced options using the JACK audio driver.

- `make linux-adv-all`: builds the external on Linux with advanced options and support for ALSA, PULSE and JACK audio drivers.

## Credits

This project thanks the following:

- Professors GE Wang and Perry Cook and all chuck and chuggin contributors for creating the amazing ChucK language and chuggin ecosystem!

- Professor Perry Cook for co-authoring Chuck and creating the [Synthesis Toolkit](https://github.com/thestk/stk) which is integrated with chuck.

- Professor [Brad Garton](http://sites.music.columbia.edu/brad) for creating the original [chuck~](http://sites.music.columbia.edu/brad/chuck~) external for Max 5. My failure to build it and run it on Max 8 motivated me to start this project.

- David Braun, the author of the very cool and excellent [DawDreamer](https://github.com/DBraun/DawDreamer) project, for creating the excellent [ChucKDesigner](https://github.com/DBraun/ChucKDesigner) project which embeds chuck in a Touch Designer plugin. His project provided a super-clear blueprint on how to embed `libchuck` in another host or plugin system and was essential to this project.
