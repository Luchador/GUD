# Character LOD distance

Under **Settings > Level Settings > Stage Options**, edit **Character LOD
factor** to set the selected stage's `LevelEntry.chrLODDistance` float.
The field accepts finite numbers greater than zero.

This is a multiplier of the distance used by the game's existing model LOD
checks, rather than one absolute distance shared by all character models:

| Factor | Low-LOD switch distance relative to the model default |
| --- | --- |
| 0.5 | Twice as far away |
| 0.8 | 1.25 times as far away; reproduces the former hard-coded GUD factor |
| 1 | The model's normal threshold |
| 2 | Half as far away |

The existing model size and view/FOV adjustments still apply. Models need
native LOD nodes for this setting to affect their appearance. GEditor's model
preview does not simulate the game's distance-based character LOD selection;
tune the value using an exported ROM.

**Apply**, **Revert edits**, and **Use ROM defaults** include the LOD field.
Applying changes marks the project unsaved. Save Project preserves the float
with round-trip precision; Create ROM writes it at offset 32 of the level row.
Rebase keeps project edits, adopts untouched incoming defaults, and reports a
conflict when both the project and new ROM change this field differently.
Solo and multiplayer catalog entries keep independent settings.

The runtime previously ignored this new field and used a hard-coded 0.8.
It now caches the selected stage's value once during loading, including any
multiplayer override. The factor applies during character matrix/LOD updates
and is reset afterward, leaving the following prop model updates unchanged.
Missing stages or invalid hand-authored factors fall back to 1. The current
`lv.c` table supplies 1 for each stage; set 0.8 to retain the prior behavior.

## Build and project format

Apply this patch to `d8838cd`, then rebuild GEditor and clean-build GUD.
The editor requires the current **44-byte LevelEntry** layout and **.gep
version 5**. Previous project formats and 36/40-byte ROM level tables are
intentionally unsupported. Create a fresh project from the rebuilt ROM;
projects created with the new editor can subsequently be rebased normally.
The manifest computes the table span using `sizeof`, so it needs no schema
version change. Clean building recompiles all native users of `lv.h`.

## Checks

```sh
python3 tools/tests/character_lod_distance/run.py
python3 tools/geditor/tests/level_memory/run.py
python3 tools/geditor/tests/current_formats/run.py
python3 tools/geditor/tests/project_rebase/run.py
# After compiling lv.o and gedmanifest.o:
python3 tools/geditor/tests/level_memory/native.py
```

These exercise stage selection, real LOD boundary calculations, character-only
scoping, form validation/defaults/dirty state, exact native offsets, project
round trips, ROM export and rebase conflicts. Final appearance is verified in
game, since the host tests do not render N64 frames.
