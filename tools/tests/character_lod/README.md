# Character LOD CPU optimization

This patch reduces repeated work in `modelUpdateMatrices`, which updates
on-screen character bodies and their attached heads. It also benefits other
models when they have the same arrangement of sibling LOD nodes.

Near/far LOD branches commonly share a parent and therefore the same joint
matrix and runtime-data base. The first branch resolves that base and computes
the scaled camera-space depth. Subsequent LOD siblings reuse both results,
while still testing their own authored minimum and maximum distances.

The cache lasts for one matrix traversal. Matrix-producing nodes invalidate it,
as do switch, BSP and head-placeholder updates. Joint callbacks, reused matrix
indices, changing heads and edited model hierarchies therefore cannot leave a
cached depth or runtime-data base in use across those updates. A different LOD
parent also forces a fresh lookup. No pointers survive between traversals,
characters, players or levels.

The original uncached LOD function remains in place for other relation-update
callers. Rendering and hit-list traversal continue to use the stored visibility
flags. This patch preserves independent body-part switching; it does not force
a character-wide LOD or change thresholds, animation, geometry, materials or
rendering options. Both distance multiplies retain their original order, and
zero-minimum, overlapping and gapped ranges retain their original behavior.

There are no model/character structure changes or persistent allocations. The
cache contains 12 bytes on N64 and is local to the matrix update. Models without
reusable sibling LODs get no lookup savings and still incur the small cache
setup/invalidation cost, so hardware timing remains the deciding measurement.

## Verification

```
TEST_CFLAGS='-fsanitize=address,undefined -fno-omit-frame-pointer' python3 tools/tests/character_lod/run.py
ASAN_OPTIONS=detect_leaks=0 TEST_CFLAGS='-fsanitize=address,undefined -fno-omit-frame-pointer' python3 tools/tests/hat_optimizations/run.py
```

The LOD suite extracts the production lookup, LOD and traversal functions. It
compares against the frozen pre-patch traversal, including visited-node order,
runtime data, child pointers and matrix-handler results. Matrix construction
is represented by deterministic handlers because the animation/matrix routines
are unchanged. Head-only assets are attached to a synthetic joint; additional
fixtures exercise nested head placeholders with separate runtime-data arrays.

Across 88,951 traversals, the paths matched on all 80 native character/head
trees, authored threshold boundaries and adjacent representable floats, varied
joint depths, zoom/model/distance scales, switches, disabled LOD, overlapping
and gapped ranges, nonfinite inputs, repeated matrix IDs and joint callbacks.

| Count within the tested traversal | Original | Cached |
| --- | ---: | ---: |
| Runtime-data lookups | 1,711,951 | 884,731 |
| Parent-chain steps in runtime/matrix lookups | 11,566,218 | 5,867,585 |
| LOD depth calculations | 1,593,552 | 797,275 |

These counts exclude the unchanged animation work inside the real matrix
handlers. They are neither CPU timings nor predicted FPS gains. For example,
snow guard LOD depth evaluations fell from 59,202 to 28,314 across its test
views; suit Bond fell from 45,000 to 22,500.

AddressSanitizer/UndefinedBehaviorSanitizer and the existing hat suite pass.
The full modified `model.c` compiles with the project's IDO 5.3 US flags to a
big-endian MIPS-II object. A complete ROM/hardware run was not performed here.

For hardware comparison, use the same camera view and AA/VI settings in a scene
with several visible guards. Train or Runway should be useful. The existing FPS
counter remains available; no temporary profiler is added.
