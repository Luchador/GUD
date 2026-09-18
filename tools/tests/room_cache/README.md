# Required room memory before optional render caches

This patch targets cache pressure in the shared `mema` heap. It applies to
GUD master `ef0bb2efc852155ff66f7d70ec9a38cdea146de5`.

Room one-cycle/cutout lists and model one-cycle lists now use a shared allocation
registry. Required room vertices and display lists continue to use `mema`
directly. Collision bounds and all original render data stay outside the
optional registry.

When a required room allocation fails:

1. Stop creating optional copies, leaving the room unloaded for retry.
2. Finish submitting the current frame. Keep every existing copy alive while
   the RSP/RDP can still reference it.
3. The main loop waits for `pendingGfx == 0` before starting another frame.
4. Clear room/model cache references, free all registered copies, and invalidate
   the renderer's leaf-list cache. This also frees old model copies whose entries
   were replaced after texture/display-list invalidation.
5. Normal room loading retries using the recovered space. Original display lists
   handle rendering for the rest of this stage visit. A new stage load enables
   optional caches again.

This deliberately uses a one-time fallback instead of repeatedly rebuilding and
purging the same caches. Normal cache use remains available until a required
allocation fails. Under pressure, original rendering may be slower and recovery
can briefly wait for queued graphics. Stage memory budgets are unchanged.

This does not change the three-room loading budget, portal traversal, or the
separate partial-load/scratch-sizing error paths. It also cannot make required
geometry fit when that data alone exceeds the available heap. The reported
Aztec room 12 event has not been reproduced in an emulator or on hardware here.

## Automated validation

Run from the repository root:

```sh
python3 tools/tests/room_cache/run.py
python3 tools/tests/model_onecycle/run.py
python3 tools/tests/bg_debug/run.py
```

The room-cache test requires Linux x86-64 (WSL is suitable), Python 3, and GCC or
Clang with ASan/UBSan. It compiles production `mema`, cache management, room
loading, room freeing, and both render passes. A low-address host heap preserves
mema's actual 32-bit pointer arithmetic. ROM transfers, conversion, collision
bounds construction, and GPU submission are controlled stand-ins. It checks:

- A required room blocked by optional copies loads after reclamation.
- No reclamation occurs while the tested main-loop admission condition has
  pending graphics; copied bytes remain intact until the simulated drain.
- Original room vertices, display lists and collision bounds survive recovery.
- Primary/secondary drawing falls back to the original lists.
- Normal frees unlink allocations; conversion failures do not leak copies.
- Empty/exhausted heaps, repeated requests, and stage resets remain safe.
- Rebuilding cannot immediately take recovered room space again.

The existing model tests now also exercise the real cache registry and verify
that both active and retired model copies are recovered. Model conversion and
material regressions pass across 443 prop and 461 weapon/hand primary lists.
The BG regression passes across 2,087 native primary/secondary streams.

All five changed/new runtime C files compile with IDO 5.3 for the US N64 target.
A complete ROM link and gameplay reproduction are not part of these checks.
The separate `render_options` suite has an existing save-format assertion that
expects `0xa0` while the current source emits `0xc0`; it fails identically on the
unmodified base and is not changed by this patch.

## Gameplay check

Build a fresh ROM and revisit Aztec room 12 and the Silo locations where rooms
appeared late. Turn away and back, and cross the adjoining portals repeatedly.
Check with AA both enabled and disabled. If the long delays persist, investigate
partial room loads and portal traversal separately; this patch isolates cache
pressure rather than claiming to fix all possible missing-room conditions.
