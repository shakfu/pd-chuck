# pd-chuck

A project to minimally embed the [chuck](https://chuck.stanford.edu) engine in a pd external.

This is an ongoing slow translation from my more mature [chuck-max](https://github.com/shakfu/chuck-max) project. 

Note that it's a bit of pain to do this in pd because there are name collisions between the puredata and chuck codebases. (see `patch.diff`), which makes it a hassle to update. The current codebase is 1.4.1.0 (numchucks)

## Status

Currently producing audio in a minimal proof-of-concept kind of way.

- [x] No errors during compilation of external
- [x] Instanciate Chuck class without errors.
- [x] Create demo with audio.


## Compilation

Only tested on macOS.

```bash
git clone https://github.com/shakfu/pd-chuck
cd pd-chuck
make osx
make lib
cd chuck~
make
```

Then open `help-chuck.pd` for a basic demo.


## Dev Notes

- Managed to get basic non-working integration example to compile without errors using (and update to 1.4.1.0) the [chuck-embed](https://chuck.stanford.edu/release/files/examples/) demo.

- Note that there were some naming collisions which had to be surmounted (e.g both pd and chuck use `t_class` and `t_array` in their codebases). I've includes a `patch.diff` which captures the minor changes applied to the **chuck v1.4.1.0 code base** to get it to work well with puredata.

- After applying the patch to the 1.4.1.0 core code, it was integrated into the `check-embed` demo as is demonstrated in this repo.

- Another strategy was to convert the `core` chuck codebase into a static library: `libchuck.a`. This is included in the modified `makefile`.

- So in summary, the current method is to:

1. Slightly patch the chuck codebase to avoid naming colllisions with PD.
2. Create a static libary of core: `libchuck.a`
3. Compile external using `libchuck.a`


