
## Building

pd-chuck is currently developed and tested on macOS and Linux. For both of these platforms, it provides a make-based frontend and uses `cmake` as the backend build system.

This project provides for building two general variants depending on the need of end-user:

1. The *base system* consists of the `chuck~` external and the base CCRMA chugins. This is already more than sufficient for more than 80% of users. The base system is relatively easy to build and only needs a c++ compiler, `cmake`, `make`, `bison` and `flex`.

2. The *advanced system* consists of the base system above plus two chugins which are relatively more challenging to build and use: `Faust.chug` and `WarpBuf.chug`. The specific needs of these chugins requires the installation of `libfaust`, `libsndfile`, `librubberband` and `libsamplerate`, and makes building this variant more involved.

On macOS full requirements for both variants these can be installed using [Homebrew](https://brew.sh) as follows:

```bash
brew install cmake bison flex autoconf autogen automake flac libogg libtool libvorbis opus mpg123 lame rubberband libsamplerate
```

On Debian Linux the requirements can be installed via:

```bash
sudo apt install build-essential cmake bison flex libsndfile1-dev libasound2-dev libpulse-dev libjack-jackd2-dev libmp3lame-dev libresample1-dev librubberband-dev
```

Each build option has a Makefile target as follows:


| Makefile target        | alias        | external | chugins | faust | warpbuf | .wav   | .mp3    | .others |
| :--------------------- | :----------- | :----: | :-------: | :---: | :-----: | :----: | :-----: | :-----: |
| `macos-base-native`    | `make`       | x      | x         |       |         | x      |         |         |
| `macos-base-universal` |              | x      | x         |       |         | x      |         |         |
| `macos-adv-brew`       | `make macos` | x      | x         | x     | x       | x      | x       | x       |
| `macos-adv-full`       |              | x      | x         | x     | x       | x      | x       | x       |
| `macos-adv-nomp3`      |              | x      | x         | x     | x       | x      |         | x       |
| `macos-adv-light`      |              | x      | x         | x     | x       | x      |         |         |
| `linux-base-alsa`      | `make`       | x      | x         |       |         | x      |         |         |
| `linux-base-pulse`     |              | x      | x         |       |         | x      |         |         |
| `linux-base-jack`      |              | x      | x         |       |         | x      |         |         |
| `linux-base-all`       |              | x      | x         |       |         | x      |         |         |
| `linux-adv-alsa`       | `make linux` | x      | x         | x     | x       | x      | x       | x       |
| `linux-adv-pulse`      |              | x      | x         | x     | x       | x      | x       | x       |
| `linux-adv-jack`       |              | x      | x         | x     | x       | x      | x       | x       |
| `linux-adv-all`        |              | x      | x         | x     | x       | x      | x       | x       |

Note: *.others* includes support for .flac, .ogg, .opus, and .vorbis formats

To get a sense of build times, the following table (for macOS builds on an M1 Macbook Air) should be illustrative:

| build command                | build time (secs)  |
| :--------------------------- | :----------------- | 
| `make macos-base-native`     | 55.074             |   
| `make macos-adv-brew `       | 1:29.24            |   
| `make macos-base-universal ` | 1:44.31            |   
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

If you have installed the prerequisites above, it should be possible the advanced system with one of the following options:

- `make macos-adv-brew` or `make macos`: build the external using the previously installed homebrew dependencies, as well as downloaded `faust` headers and a downloaded pre-compiled `libfaust` (`libfaustwithllvm`) library. This is the newer, faster, recommended way of getting a full chuck-max system up and running.

- `make macos-adv-full`: build the external by manually downloading and building all of the dependencies except for `libfaust` from source. This is the previous way of building an advanced system. It is currently only for advanced developers who want maximum flexibility in their builds.

- `make macos-adv-nomp3`: same as `make full` except without support for the .mp3 format. Try this variant if you are unable to build using `make full` on Intel Macs or on older macOS versions.

- `make macos-adv-light`: Same as `make full` except for withouth `libsndfile` multi-file format support. This means that (.mp3, flac, vorbis, opus, ogg) formats are not supported in this build. Only `.wav` files can be used.

- `make linux-adv-alsa` or `make linux`: builds the external on Linux using the default ALSA audio driver. This is the default advanced option for Linux.

- `make linux-adv-pulse`: builds the external on Linux with advanced options using the PULSE audio driver.

- `make linux-adv-jack`: builds the external on Linux with advanced options using the JACK audio driver.

- `make linux-adv-all`: builds the external on Linux with advanced options and support for ALSA, PULSE and JACK audio drivers.


