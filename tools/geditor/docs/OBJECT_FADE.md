# Per-object fade distances

Select an object, check **Use custom fade distances**, enter **Fade start (m)**
and **Fade end (m)**, then click **Apply fade distances** (or press Enter in
either distance field). Both values apply together. Escape in either field
restores the saved pair. Uncheck the option and Apply to restore level defaults.

The object is fully visible through the start distance, fades linearly between
the two distances, and stops rendering at the end distance. Distances measure
from the active camera to the object's origin in gameplay world space, including
cutscenes and each multiplayer view. They are fixed distances, independent of
model size and FOV. The range is 0–655.35 metres in 0.01-metre increments; end
must exceed start. The initial suggested pair is 20/25 metres.

This overrides the environment's screen-size fade for that object. Guards keep
their existing fade settings. Object ticking, drone-gun targeting, collision,
regeneration, and room/portal visibility continue normally. Fog, existing material
transparency, and other visibility constraints can still hide an object sooner.
GEditor retains its normal editing preview; these distances control the game.

Save/reopen, undo/redo, object duplication, setup compaction, export and rebase
retain the override. Rebuild GUD and GEditor, then rebase the project onto the
new GUD ROM before exporting custom fades. Export rejects an older runtime that
would ignore them. Existing setups need no format conversion.

## Encoding

`src/objectfadeformat.h` defines the shared format. Two runtime matrix words
inside the existing 128-byte ObjectRecord prefix carry authored data:

| Offset | Value |
| --- | --- |
| 0x28 | `GFD1` (0x47464431) |
| 0x2c | Unsigned 16-bit start centimetres followed by unsigned 16-bit end centimetres |

Untagged records use environment defaults. Clearing an override zeros both
words. `objInit` captures them before placement replaces the matrix. The unused
word at PropRecord offset 0x30 now holds two unsigned 16-bit distances, retaining
the existing 52-byte record and 600-entry pool size. Allocation clears both
values so reused props cannot inherit another object's settings. Invalid tagged
ranges fall back to defaults in-game and are rejected at export.

The `OFAD` manifest capability (version 1) identifies compatible runtimes.
Rebase validates support against the destination ROM so an old project can
upgrade without first removing its overrides.
