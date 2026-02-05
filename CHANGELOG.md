# Changelog

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.0.0/) and [Commons Changelog](https://common-changelog.org). This project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## Types of Changes

- Added: for new features.
- Changed: for changes in existing functionality.
- Deprecated: for soon-to-be removed features.
- Removed: for now removed features.
- Fixed: for any bug fixes.
- Security: in case of vulnerabilities.

---

# [0.1.x]

- Added AbletonLink chugin with auto-fetch of Link SDK from GitHub (requires `-DENABLE_ABLETONLINK=ON`)

- Added AudioUnit chugin for hosting Audio Unit plugins (macOS only, built automatically)

- Added CLAP chugin for hosting CLAP plugins (requires `-DENABLE_CLAP=ON`)

- Added VST3 chugin for hosting VST3 plugins with auto-fetch of VST3 SDK from GitHub (requires `-DENABLE_VST3=ON`)

- Added configurable I/O channels: `[chuck~ channels]` or `[chuck~ channels tap_channels]`

- Added tap infrastructure for reading global UGen samples via tap outlets

- Added `tap` message for setting global UGens to tap (reads samples from named UGens)

- Added `removeall` message to remove all shreds while keeping VM state

- Added `adaptive` message to get/set adaptive mode for the VM shreduler

- Added `param` message to get/set ChucK VM parameters

- Added `shreds` message for shred introspection (subcommands: all, ready, blocked, count, highest, last, next, or query by ID)

- Added `is_safe_path()` security function to validate paths against traversal attacks

- Changed `ck_edit()` to use fork/exec on macOS for safer command execution

- Updated chuck code base to `1.5.5.8-dev (chai)`

- Added cmake function `add_chugin()` 

- Added support for `XML.chug`

- Added support for `Ladspa.chug`

- Added support for `FluidSynth.chug` via `build_fs` make target.

- Updated chuck code base to `1.5.5.1-dev (chai)`

- Updated chuck code base to `1.5.4.5-dev (chai)`

- Updated chuck code base to `1.5.4.3-dev (chai)`

- Added `ConvRev` and `Line` chugins

- Updated chuck code base and examples to `1.5.4.2-dev (chai)`

- Updated chuck code base to `1.5.3.2-dev (chai)`

- Updated chuck code base to `1.5.2.6-dev (chai)`

- Fixed `Faust.chug` bug due to faust stdlib not being stored due to `.gitignore` misconfig.

- Changed `remove` method, it can now remove multiple space separated shred_ids

- Added improved build system via additional build scripts to handle corner cases.

- Added `listen` and `unlisten` to event related callbacks

- Added get/set methods to get global var values via callbacks and to set the values of the same directly

- Added `globals` message to provide a list of globals in the console

- Added `vm` message to provide status of vm

- Added `docs` message to open a browser window to the ChucK docs site

- Dropped `info` method and merged its functionality into `status`.

- Added `editor` and `edit` methods for setting external text editor and editing specified files as well as previously run files.

- Added argument parsing to `add` here args are space separated and not colon separated.

- Added `tests` folder for feature tests

- Added `eval` message (with test) to compile chuck code in puredata message

- Added `chugins` message to probe and list chugins in the console

- Updated chuck codebase to ChucK 1.5.2.5-dev (chai)

- Added `external_dir`, which contains the external binary and made the `examples_dir` relative to it, instead of previously when it was relative to the `patch_dir`.

- Added `loglevel` / `loglevel <int>` message for setting chuck log level 0-10.

- Added a number of missing chuck messages: `add <filepath>`, `replace <shredID> <filepath>`, `reset id`, `clear vm`, `clear globals`, `time`, `status` along with their respective symbols: `+`, `-`, `--`, `=`, `^`, etc.

- Added support for building the `Faust` or `Fauck` chugin with the `faust`
stdlib in `pd-chuck/chuck_tilde/examples/faust`

- Added support for building the `WarpBuf` chugin

- Changed the content and structure of the `examples` folder to more closely resemble the updated chuck examples folder. See the `README.md` file in the folder for a more granular list of changes.

- Removed posix header includes and replaced with x-platform code in aid of eventual windows support

- Added callback mechanism and example

- Added examples for local and global event triggering

- Fixed the `sndbuf.ck` exmample (and added `sndbuf1.ck`) to demonstrate wav file playback via `SndBuf` with some samples

- Changed `signal` message name to `sig` as it conflicted with a global Max message `signal`

- Changed `info`: now shows `<object id> - <shred-id>`

- Added test for two object instances running simultaneously

- Added `info` message to list running shreds in the console

- Update to chuck 1.5.2.3-dev (chai)


# [0.1.0]

- Initial support for chuck 1.5.1.3

- Support for Linux

- Support for macOS

