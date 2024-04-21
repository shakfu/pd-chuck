# WarpBuf

With WarpBuf you can time-stretch and independently transpose the pitch of an audio file. Only .wav files are supported. If you don't have an Ableton `.asd` file to go with the audio file, then the BPM will be assumed to be 120. Therefore, to play the file twice as fast, do `240. => myWarpBuf.bpm;` Any mono channel UGen can be chucked to `.bpm` too.

Because the audio is being read from the file system one sample at a time, you can avoid artifacts by using a larger buffer size: `chuck --bufsize2048 foo.ck`

Control parameters:
* .read - ( string , WRITE only ) - loads file for reading
* .playhead - ( float , READ/WRITE ) - set/get playhead position in quarter notes relative to 1.1.1
* .loop - ( int , READ/WRITE ) - toggle looping
* .play - ( int , READ/WRITE ) - toggle playing
* .bpm - ( float , READ/WRITE ) - set/get BPM rate ( Audio files without Ableton warp files are assumed to be 120 BPM )
* .transpose - ( float , READ/WRITE ) - set/get pitch transpose in semitones
* .startMarker ( float , READ/WRITE ) - set/get start marker of the clip
* .endMarker ( float , READ/WRITE ) - set/get end marker of the clip
* .loopStart ( float , READ/WRITE ) - set/get loop start marker of the clip
* .loopEnd ( float , READ/WRITE ) - set/get loop end marker of the clip
* .reset ( float , WRITE ) - reset the internal process buffer of the Rubberband stretcher

## Ableton Live Beatmatching

With WarpBuf, you can also use Ableton Live `.asd` files to [warp](https://www.ableton.com/en/manual/audio-clips-tempo-and-warping/) audio files. The warp markers and implicit BPM information in the `.asd` file will affect how ChucK plays the file. The `.asd` should be next to the `.wav` file, so you might have `drums.wav` and `drums.wav.asd`.

Two audio files might have different tempos, but you can "beatmatch" them by giving them the same tempo:

```chuck
WarpBuf warpBuf1 => dac;
WarpBuf warpBuf2 => dac;
130. => warpBuf1.bpm => warpBuf2.bpm;
```

WarpBuf has been tested with `asd` files created with Ableton Live 9 and 10.1.30.

## Installation

Make sure you have `cmake`, `git`, and `sh` available from the command line/Terminal. On macOS/Linux, you also need `pkg-config`.

Update submodules:
`git submodule update --init --recursive`

### Windows

Create an extra folder for your chugins, `%USERPROFILE%/Documents/ChucK/chugins/`. Create a system environment variable `CHUCK_CHUGIN_PATH` equal to this path. In `chugins/WarpBuf`, open a command window and run `call build_windows.bat`. The `WarpBuf.chug` should appear inside `%USERPROFILE%/Documents/ChucK/chugins/` and `chugins/WarpBuf/package`.

### Ubuntu

Install dependencies:

```bash
sudo apt install autoconf autogen automake build-essential libasound2-dev libflac-dev libogg-dev libtool libvorbis-dev libopus-dev libmp3lame-dev libmpg123-dev pkg-config python
```

In `chugins/WarpBuf`, open a Terminal window and run `sh build_unix.sh`. The `WarpBuf.chug` should appear inside `chugins/WarpBuf/package`.

### macOS

Install dependencies with brew:

```zsh
brew install autoconf autogen automake flac libogg libtool libvorbis opus mpg123 pkg-config
```

In `chugins/WarpBuf`, open a Terminal window and run `sh build_unix.sh`. The `WarpBuf.chug` should appear inside `chugins/WarpBuf/package`.

## Testing

Run any of the test scripts: `chuck.exe --verbose:10 level:10 "tests/warpbuf_basic.ck"`.

## Licenses

WarpBuf uses [Rubber Band Library](https://github.com/breakfastquay/rubberband/) and [libsamplerate](https://github.com/libsndfile/libsamplerate), so your usage of WarpBuf must obey their licenses.

## Todo:

* Get/set the list of warp markers.
* Optionally pre-read the entire audio buffer and hold it in a buffer.