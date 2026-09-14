# Character hat CPU optimizations

This incremental patch follows the background transparency override patch.
It implements the first three proposed hat optimizations:

1. Build ordinary attached-hat matrices directly, combining the root and
   head-fitting translations before applying them to the current head matrix.
2. Cache the fixed fitting offsets, scale, attachment matrix index and model
   classification in runtime character data.
3. Carry the hit-list tail through body/weapon/hat construction, and append a
   validated rigid hat's single sortable root without another tree traversal.

All twelve stock hat assets have one matrix and a root -> bounding box ->
display-list hierarchy. The fast path checks that structure rather than
assuming a stock model ID guarantees it. Edited models with extra joints,
switches, LODs or other structures retain generic matrix/list construction.
The fallback applies head fitting to matrix zero after the generic update,
preserving the previous order for multi-matrix models.

The cache adds 52 bytes to each N64 `ChrRecord`, including background AI
records. It resets on character initialization, hat assignment and detachment,
and refreshes when the hat model/header/root, body header, attachment node,
head number or hat number changes. Cached fitting does not retain a camera
matrix; every view/frame uses the current body matrices and current local root
origin. Model hierarchies and attachment matrix indices are otherwise treated
as immutable while attached, as in the existing asset pipeline.

`HatRecord`, setup assets, model slots and hit entries keep their formats.
The existing Makefile dependency on `bondtypes.h` rebuilds game consumers of
the changed runtime character stride. Rebuild the game normally after applying
the patch; exported GEditor projects need the new ROM as their engine base.

The original hit-entry allocation order, sorting and pool-exhaustion behavior
are preserved. Legacy menu/title list callers retain their compatibility
entry point. Held weapons use a typed `ModelHitList` builder instead of casting
a hit-chain pointer to `Gfx **`.

Unknown/custom hat IDs no longer index the fitting table with
`HATTYPE_OTHER == -1`. They use their authored attachment transform without a
stock fitting correction. Peaked-hat head visibility and drop-frame matrix
generation retain their existing rules. This patch does not implement the
proposed secondary-pass or one-cycle hat rendering changes.

## Validation

From the repository root:

```sh
python3 tools/tests/hat_optimizations/run.py
ASAN_OPTIONS=detect_leaks=0 UBSAN_OPTIONS=halt_on_error=1 TEST_CFLAGS='-fsanitize=address,undefined -fno-omit-frame-pointer' python3 tools/tests/hat_optimizations/run.py
python3 tools/tests/gameplay_optimizations/run.py
```

The hat suite compiles production fitting, transform, list and held-weapon
functions. It verifies all 12 asset hierarchies, 10,752 synthetic poses across
all 168 head-fitting entries, identity/unknown fits, moving views, invalidation,
live root offsets, extra-joint fallback, drop frames and both held-weapon hands.
List construction is compared with the frozen pre-patch implementation through
empty, partially exhausted and fully exhausted pools, sorting and release.

Reassociating the two translations changes floating-point rounding slightly.
The largest observed difference in these synthetic poses was 0.001953125 in a
root translation component; scaled basis values and extra-joint matrices
matched exactly. This is a numerical comparison, not an N64 timing result.
Host sanitizer and gameplay regression checks pass. The modified game modules
and both character allocation modules compile with IDO 5.3 for the US target.
A complete ROM build is unavailable here because base-ROM/texture assets are
missing.

On hardware, compare the same view of hat-wearing guards before/after. Check
berets, fur hats, helmets and peaked hats while turning, firing and dying;
shoot hats off and verify helmet ricochets and bullet impacts. Include a level
restart and split-screen views, since cached fitting must survive model reuse
and use each player's current matrices. The existing FPS/bottleneck display
remains available for the comparison.
