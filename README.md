# pd-chuck


This repo was created to capture my ongoing attempts to wrap chuck 1.4.1.0 (numchucks) in a pure-data external.

My development platform is macOS Catalina 10.15.7


## Status

- Current PD external is currently non-functional but promising proof-concept.

- [x] No errors during compilation of external
- [x] Instanciate Chuck class without errors.
- [ ] Create demo with audio. May have to include RTAudio as well.


## Dev Notes

- Managed to get basic non-working integration example to compile without errors using (and update to 1.4.1.0) the [chuck-embed](https://chuck.stanford.edu/release/files/examples/) demo.

- Note that there were some naming collisions which had to be surmounted (e.g both pd and chuck use `t_class` and `t_array` in their codebases). I've includes a `patch.diff` which captures the minor changes applied to the **chuck v1.4.1.0 code base** to get it to work well with puredata.

- After applying the patch to the 1.4.1.0 core code, it was integrated into the `check-embed` demo as is demonstrated in this repo.

- Another strategy was to convert the `core` chuck codebase into a static library: `libchuck.a`. This is included in the modified `makefile`.

- The puredata external is tentatively called `puck` to avoid naming collisions during development (but has the alias 'chuck' in any case).

- So in summary, the current method is to:

1. Slightly patch the chuck codebase to avoid naming colllisions with PD.
2. Create a static libary of core: `libchuck.a`
3. Compile external using `libchuck.a`

The following section covers the above.


## Steps to Create the PD-Chuck External


1. Compile the `chuck-embed` executable

```bash
make osx
```

2. Create the static library from just created object files

```bash
make libchuck.a
```

3. Compile the puredata external

```bash
cd puck
make
```

4. Run the `help-puck.pd` example.

