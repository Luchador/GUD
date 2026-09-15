# Model one-cycle regression

Run from the repository root:

```sh
python3 tools/tests/model_onecycle/run.py
ASAN_OPTIONS=detect_leaks=0 UBSAN_OPTIONS=halt_on_error=1 TEST_CFLAGS='-fsanitize=address,undefined -fno-omit-frame-pointer' python3 tools/tests/model_onecycle/run.py
```

The harness compiles the production model material setup, alternate-list cache,
both model-node draw paths, shared converter and AA submission walker. It checks
opaque fog/depth state, internal decal/cutout/translucent exclusions, exact
outgoing state, per-instance colors, secondary calls, transient-list exclusion,
AA/VI queue gating, cache reuse, source replacement, pending-copy retention,
stage reset, allocation failure and both cache limits.

Damaged-prop checks independently evaluate the original two-cycle and emitted
one-cycle alpha muxes using nine-bit RDP arithmetic for all 32,768 combinations
of eligible environment alpha and vertex alpha. They verify the same alpha
and zero/nonzero hole mask, the base-texture/shade RGB mux, fog/depth state,
all four native damage levels, intact/damaged cache isolation, per-instance
damage/fog values, mixed-list restoration and unsupported-material fallback.
AA On must retain the original setup and list.

It also walks the repository's real prop models and checks their primary
display lists, including Train's wooden and metal crates. Both console2 and
console3 must convert all 20 body triangle packets at every native damage
level. Multi-byte GBI words
and pointers are adapted for the host. Asset texture markers become representative
upload commands; texture allocation, actual TMEM contents and rasterization are
not emulated. Hardware testing is still needed for appearance and frame rate.
