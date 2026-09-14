# Bullet hit texture regression

Run from the repository root:

```sh
python3 tools/tests/hit_textures/run.py
ASAN_OPTIONS=detect_leaks=0 TEST_CFLAGS='-fsanitize=address,undefined -fno-omit-frame-pointer' python3 tools/tests/hit_textures/run.py
```

The harness compiles the production texture descriptor lookup, background and
object ray intersection functions, light texture predicate, and background impact
dispatch. It checks both TRI1 and TRI4, every recognized lamp texture, ordinary
and custom materials, the Archives light-shaft exclusion, untextured fallback,
and primary/secondary object display lists.

N64 packet reads and address translations are adapted for the host. Audio,
particles, and `lightFixtureBreak` are observers: the test verifies that the real
impact dispatch calls the break routine with the original triangle and room, but
does not emulate rendering or verify light darkening on hardware.

The former `texture data - 8` lookup reads `0x4755` from the current metadata
marker instead of the texture ID. To reproduce that failure, pass a directory
containing the pre-fix `src/game/bg.c` and `src/game/propobj.c` as the optional
argument to `run.py`. The first lamp texture assertion should fail.
