# Character LOD smoothing-matrix skips

This change builds on GUD master `6489925` (Animation caching).

The model traversal already skips invisible LOD geometry. The remaining
opportunity addressed here is the half-rotation matrices used to smooth
high-detail joints: ordinary body groups build these even when all their
consuming display lists are hidden by LOD.

For CarmourguardZ, the far pose omits five smoothing transforms (matrix indices
9, 13, 16, 19 and 20), while preserving the other sixteen transforms. This is
not a 24% frame-rate improvement: animation decoding, primary joint transforms,
hit detection, matrix conversion, rendering and other game work still run.

The optimization is scoped to the body pose in `chrTick`. It:

- Caches authored matrix-load dependencies, including currently hidden branches.
- Tests current joint depths with the same LOD thresholds, scale and zoom rules.
- Requires each dependency's depth matrix to be the current joint or an ancestor,
  with unique matrix producers outside conditional branches.
- Keeps the primary matrix's arithmetic and joint callbacks unchanged in both
  ordinary and animation-blending paths.
- Checks attached heads, per-instance display lists and cache lifetime.
- Initializes unused matrix slots to identity for the existing float-to-fixed
  conversion, instead of leaving uninitialized float data.
- Stops additional skip checks after finding a needed near branch. Nearby poses
  retain their detail; this conservative early exit can miss some safe skips in
  mixed poses.

Unsupported hierarchies, nested display lists, matrix/segment overrides,
unconditional consumers, too many dependencies and cache exhaustion fall back
to the original calculations. No character or model-file layout changes.

The IDO US build adds 7,584 bytes of static metadata and 3,848 bytes of code and
read-only data in `modellod.o`, plus small call-site changes. There is no heap
allocation per character. Instance validation is cached to avoid rescanning all
body lists every frame. Any future code that replaces an instance's display
lists must call `modelLodInvalidateInstance`; current initialization and monitor
replacement paths do so. Model loading, display-list replacement, texture-list
rewrites and stage/render-cache resets invalidate the asset metadata as well.

## Run

From the repository root, with Python 3 and GCC or Clang:

```sh
python3 tools/tests/character_lod_skips/run.py
TEST_CFLAGS='-fsanitize=address,undefined -fno-omit-frame-pointer' \
  python3 tools/tests/character_lod_skips/run.py
```

The test extracts the production dependency analysis and both actual joint
builders. Only host pointer width and the simulated RDRAM address mapping are
adapted. It reads all 80 bundled character/head binaries, tests all 43 body
hierarchies against an independent visible-display-list matrix-use oracle,
and exercises attached heads separately.

Checks cover 11,061 poses with varied per-joint depths, zoom and model scales;
exact LOD boundaries and adjacent floats; disabled LOD; both quaternion paths;
unchanged primary matrices and near smoothing matrices; custom head dependencies;
runtime display-list replacement; unsupported commands; cache exhaustion;
invalidation; and source/linker integration. Existing `character_lod`,
`hat_optimizations`, `model_onecycle` and `destroyed_props` suites also pass.

All changed game C files compile with the project's IDO compiler. A targeted
instruction trace of the compiled MIPS joint builders also confirmed fewer
instructions for a warm-cache far CarmourguardZ pose, with a small checking cost
for a near pose. This is not a cycle-accurate GPU/emulator or hardware FPS test.
In-game testing should compare distant guard groups, approaching a guard across
the LOD boundary, scoped zoom, death fades, head/hat variants and held weapons.
