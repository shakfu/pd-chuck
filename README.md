# pd-chuck

A project to minimally embed the [chuck](https://chuck.stanford.edu) engine in a pd external.

This is an ongoing translation from my more mature [chuck-max](https://github.com/shakfu/chuck-max) project. 

Note that it's a bit of pain to do this in pd because there are name collisions between the puredata and chuck codebases. (see 'Dev Notes' below), which makes it a hassle to update. The current chuck version used is `1.5.0.1-dev`

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
make mac
make lib
cd chuck~
make
```

Then open `help-chuck.pd` for a basic demo.


## Dev Notes

- Managed to get basic proof-of-concept integration with audio output and basic example to compile without errors using chuck version `1.5.0.1-dev`.

- Note that there were some naming collisions which had to be surmounted (e.g both pd and chuck use `t_class` and `t_array` in their codebases). The following changes were made to the chuck codebase:

```c++

t_array -> t_carray
t_class -> t_cclass
mtof 	-> ck_mtof
ftom 	-> ck_ftom
rmstodb -> ck_rmstodb
powtodb -> ck_powtodb
dbtopow -> ck_dbtopow
dbtorms	-> ck_dbtorms
```

- These changes are applied to the `core` and `host` folders by a script (`scripts/transform.sh`) for automation. The script requires the [rpl](https://pypi.org/project/rpl/) search and replace utility which can be installed via:

```bash
pip install rpl
```

- Another strategy to speed up recompilation of the external useing development was to convert the `core` chuck codebase into a static library: `libchuck.a`. This is included in the modified `makefile`.

- So in summary, the current method for a new chuck codebase is to:

1. Slightly patch the chuck codebase to avoid naming colllisions with PD.
2. Create a static libary of core: `libchuck.a`
3. Compile external using `libchuck.a`


