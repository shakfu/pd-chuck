# Changelog

All notable changes to this project are documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/)
and [Common Changelog](https://common-changelog.org), and this project adheres
to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

This project has no release tags of its own; each section below tracks the
version of the vendored ChucK codebase, dated by the commit that adopted it.

<!--
Change groups, in order:
  Added      - for new features.
  Changed    - for changes in existing functionality.
  Deprecated - for soon-to-be removed features.
  Removed    - for now removed features.
  Fixed      - for any bug fixes.
  Security   - in case of vulnerabilities.
-->

## [1.5.5.9-dev] - 2026-07-21

### Added

- Added `reply` message and a dedicated rightmost reply outlet for routing ChucK global callbacks (get/listen/shred replies) back into the patch. Opt-in (`reply 1`): ChucK's globals callbacks fire on the audio thread, so replies are queued on a lock-free per-instance ring and drained on the scheduler thread by a clock. Replies use a reserved selector vocabulary (`val`, `event`, `shred`, `global`) so no ChucK global name can collide.

- Added `verbose` message to control pd-side reporting verbosity, kept distinct from `loglevel` (which controls the ChucK VM's own process-global log level via `ChucK::setLogLevel()`).

- Added `abort` message to abort the currently-running shred.

- Added examples and tests for the new messaging: `test_reply.pd`, `test_verbose.pd`, `test_loglevel.pd`, and `reply_test.ck`, plus additional example patches (`noise-machine`, `ambisonic-encoding`, `spherical-harmonics`, `golden`, `paulstretch`).

### Changed

- Updated ChucK codebase to `1.5.5.9-dev (chai)`.

## [1.5.5.8-dev] - 2026-01-14

### Added

- Added PdPatch chugin for loading and running Pure Data patches inside ChucK (requires `-DENABLE_PDPATCH=ON`). Wraps libpd (auto-fetched via CMake FetchContent) with stereo audio I/O, bidirectional messaging (send/receive float, bang, symbol, list, typed message), Pd array read/write access, and MIDI send. Supports multiple independent Pd instances. Added `build_pd` and `test-pdpatch` make targets.

- Added AbletonLink chugin with auto-fetch of Link SDK from GitHub (requires `-DENABLE_ABLETONLINK=ON`).

- Added AudioUnit chugin for hosting Audio Unit plugins (macOS only, built automatically).

- Added CLAP chugin for hosting CLAP plugins (requires `-DENABLE_CLAP=ON`).

- Added VST3 chugin for hosting VST3 plugins with auto-fetch of VST3 SDK from GitHub (requires `-DENABLE_VST3=ON`).

- Added configurable I/O channels: `[chuck~ channels]` or `[chuck~ channels tap_channels]`.

- Added tap infrastructure for reading global UGen samples via tap outlets.

- Added `tap` message for setting global UGens to tap (reads samples from named UGens).

- Added `removeall` message to remove all shreds while keeping VM state.

- Added `adaptive` message to get/set adaptive mode for the VM shreduler.

- Added `param` message to get/set ChucK VM parameters.

- Added `shreds` message for shred introspection (subcommands: all, ready, blocked, count, highest, last, next, or query by ID).

### Changed

- Updated ChucK codebase to `1.5.5.8-dev (chai)`.

### Security

- Added `is_safe_path()` security function to validate paths against traversal attacks.

- Changed `ck_edit()` to use fork/exec on macOS for safer command execution.

## [1.5.5.7-dev] - 2025-12-27

### Changed

- Updated ChucK codebase to `1.5.5.7-dev (chai)`.

## [1.5.5.3-dev] - 2025-08-01

### Changed

- Updated ChucK codebase to `1.5.5.3-dev (chai)`.

## [1.5.5.2-dev] - 2025-05-31

### Changed

- Updated ChucK codebase to `1.5.5.2-dev (chai)`.

## [1.5.5.1-dev] - 2025-03-22

### Added

- Added cmake function `add_chugin()`.

- Added support for `XML.chug`.

- Added support for `Ladspa.chug`.

- Added support for `FluidSynth.chug` via `build_fs` make target.

### Changed

- Updated ChucK codebase to `1.5.5.1-dev (chai)`.

## [1.5.4.5-dev] - 2025-02-04

### Changed

- Updated ChucK codebase to `1.5.4.5-dev (chai)`, moving through `1.5.4.3-dev`.

## [1.5.4.2-dev] - 2024-11-05

### Added

- Added `ConvRev` and `Line` chugins.

### Changed

- Updated ChucK codebase and examples to `1.5.4.2-dev (chai)`.

## [1.5.3.2-dev] - 2024-10-09

### Changed

- Updated ChucK codebase to `1.5.3.2-dev (chai)`.

## [1.5.2.6-dev] - 2024-07-25

### Changed

- Updated ChucK codebase to `1.5.2.6-dev (chai)`.

## [1.5.2.5-dev] - 2024-05-18

### Added

- Added improved build system via additional build scripts to handle corner cases.

- Added `listen` and `unlisten` to event related callbacks.

- Added get/set methods to get global var values via callbacks and to set the values of the same directly.

- Added `globals` message to provide a list of globals in the console.

- Added `vm` message to provide status of vm.

- Added `docs` message to open a browser window to the ChucK docs site.

- Added `editor` and `edit` methods for setting external text editor and editing specified files as well as previously run files.

- Added `tests` folder for feature tests.

- Added `eval` message (with test) to compile chuck code in puredata message.

- Added `chugins` message to probe and list chugins in the console.

### Changed

- Changed `remove` method: it can now remove multiple space separated shred_ids.

- Changed `add` argument parsing so args are space separated and not colon separated.

- Updated ChucK codebase to `1.5.2.5-dev (chai)`.

### Removed

- Dropped `info` method and merged its functionality into `status`.

### Fixed

- Fixed `Faust.chug` bug due to faust stdlib not being stored due to `.gitignore` misconfig.

## [1.5.2.3-dev] - 2024-04-02

### Added

- Added `external_dir`, which contains the external binary and made the `examples_dir` relative to it, instead of previously when it was relative to the `patch_dir`.

- Added `loglevel` / `loglevel <int>` message for setting chuck log level 0-10.

- Added a number of missing chuck messages: `add <filepath>`, `replace <shredID> <filepath>`, `reset id`, `clear vm`, `clear globals`, `time`, `status` along with their respective symbols: `+`, `-`, `--`, `=`, `^`, etc.

- Added support for building the `Faust` or `Fauck` chugin with the `faust` stdlib in `pd-chuck/chuck_tilde/examples/faust`.

- Added support for building the `WarpBuf` chugin.

- Added callback mechanism and example.

- Added examples for local and global event triggering.

- Added test for two object instances running simultaneously.

- Added `info` message to list running shreds in the console.

### Changed

- Changed the content and structure of the `examples` folder to more closely resemble the updated chuck examples folder. See the `README.md` file in the folder for a more granular list of changes.

- Replaced posix header includes with cross-platform code in aid of eventual windows support.

- Changed `signal` message name to `sig` as it conflicted with a global Max message `signal`.

- Changed `info`: now shows `<object id> - <shred-id>`.

- Updated ChucK codebase to `1.5.2.3-dev (chai)`.

### Fixed

- Fixed the `sndbuf.ck` example (and added `sndbuf1.ck`) to demonstrate wav file playback via `SndBuf` with some samples.

## [0.1.0] - 2023-09-03

### Added

- Initial support for chuck 1.5.1.3.

- Support for Linux.

- Support for macOS.
