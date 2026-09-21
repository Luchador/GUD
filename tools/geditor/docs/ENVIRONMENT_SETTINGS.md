# Environment overrides

Open **Settings > Level Settings > Environment**. Choose the environment row,
edit its values, and click **Apply** (or press Enter). Fog, background color and
cloud changes preview in the viewport. Leaving the tab restores the level's
normal gameplay preview. Water, visibility culling and prop fading are exported
but are not simulated by this viewport preview.

The form exposes the following `EnvironmentRecord` fields:

| Section | Fields |
| --- | --- |
| Clipping and fog | `fogEnabled`, `nearclip`, `farclip`, `intensity`, `fogstart`, `fogend` |
| Sky and colors | Fog RGB, clouds on/off, `skyheight`, sky image ID, cloud RGB |
| Water and prop fade | `iswater`, `waterheight`, `waterid`, water RGB, `horizonyoffset`, `propStartFade`, `propEndFade` |

RGB values use 0–255; cloud and water colors retain fractional values. Sky and
water image IDs address the existing three-entry sky/water image bank (0–2).
Fog start/end use the game's projected-depth units, rather than world distances;
values above 1000 are allowed, including Bunker 2's authored 1050 fog end.
Near clip must be positive, far clip must exceed it, and enabled fog must have
a representable, increasing range. A positive prop fade start requires a lower
fade end. Zero start uses engine defaults; a negative start disables prop fading.

**Revert edits** discards un-applied text changes. **Use ROM defaults** removes
that row's visible project overrides and restores the current base ROM's values.
Save Project, Create ROM, switching levels/variants/tabs, and closing Level
Settings apply valid pending edits. Invalid values leave the draft in place
and block that operation until corrected or reverted. Applied changes mark the
project unsaved. Applied environment edits are project metadata, outside the
geometry undo history; Ctrl+Z in an edit box still undoes text entry.

The selector exposes the existing gameplay, alternate, cinema and multiplayer
rows relevant to the selected level. Multiplayer catalog entries offer their
two-, three- and four-player environments. A **shared default** label means
that editing the row affects every level using that fallback. No table rows
are added or reordered. The game's normal row selection and scripted
interpolation still determine which settings take effect during play.

Only fields differing from the base ROM are saved, keyed by environment ID and
field name in the `.gep`. Export changes those fields in the ROM's native
`ENVT` table, preserving IDs, reserved bytes, row order and the terminator.
The project's `base.z64` is unchanged. Repeated edits/exports do not append data;
restoring defaults removes obsolete overrides.

Projects with environment overrides use **GEditor Project 3** (or version 4
when memory overrides are also present), so older editors reject them instead
of silently dropping the settings. Projects without overrides still save as
version 2, and all three versions open in this editor. The GUD ROM manifest
version does not change. Rebase merges each environment field separately:
project-only edits survive, untouched values adopt new ROM defaults, matching
edits are accepted, and conflicting edits or removed edited rows stop rebase.

The accompanying game fix keeps scripted environment transitions valid when
fog is disabled, finds alternate rows by ID, and safely switches the fog flag
when a transition changes it. Rebuild GUD and rebase existing projects onto that
ROM to use the runtime fix. Updating GEditor alone cannot update code retained
inside an existing project's base ROM.

## Experiment: remove legacy prop fading

The experimental patch removes **Max visibility**, **Max obfuscation** and
**Min visibility** from the form. **Near fog** is also removed because it
belongs exclusively to the same retired distance-based prop-fade system.
Their runtime calculations, early size-adjusted cutoff, cached settings
pointer and unused function arguments are removed. Objects and characters
retain GUD's screen-size fading. Fog color/start/end, clipping, far-fog culling,
glass/door opacity and portal updates, and item respawn fading remain active.

For reversal, the four old floats remain reserved in the 104-byte `ENVT`
record. Their default bytes and saved project overrides are retained, including
when editing other fields or using **Use ROM defaults**. No project/manifest
version changes are required, and rebuilding does not change default assets.

Rebuild GUD and GEditor, then rebase a project onto the experimental ROM before
exporting the test ROM. Compare Jungle at the same locations, camera angles and
settings, watching distant props/guards, fade/pop-in and FPS. Removing the old
cutoff saves its calculations but can leave more models visible, so performance
can improve or worsen depending on which fade system was limiting that scene.

To undo the source patch (from the repository root):

```sh
git apply -R GUD-remove-legacy-prop-fade-experiment.patch
```

Then rebuild both GUD and GEditor and rebase onto that restored GUD ROM.
Existing exports retain their embedded game code; reversing only the source
patch cannot change them. The preserved legacy settings become active again.

## Verification

```sh
python3 tools/geditor/tests/environment/run.py
python3 tools/geditor/tests/current_formats/run.py
python3 tools/geditor/tests/project_rebase/run.py
make -C tools/geditor/src
```

Native ASan/UBSan checks cover every field's byte layout and round trip,
validation, reset/compaction, unchanged ROM bytes, repeated export, form draft
lifetime, previews, three-way merges and runtime transitions. The rebase suite
also saves/reopens overrides and exports them through the complete ROM builder
with relocated tables. The experiment checks Jungle's removed size-adjusted
cutoff, retained fog/screen-size fading and preservation of hidden overrides.
Interactive layout/focus and in-game visuals require
Windows and emulator/hardware testing.
