# Level memory allocations

Open **Settings > Level Settings > Stage Options** to edit the selected
level's budgets. Values are whole KiB (1 KiB = 1024 bytes).

| Setting | Runtime token | Allocation |
| --- | --- | --- |
| Display lists | `-mgfx` | Size of **each** of the two master display-list buffers |
| Vertices and matrices | `-mvtx` | Size of **each** of the two auxiliary graphics buffers |
| Textures | `-mt` | Texture pool |
| Dynamic heap | `-ma` | `mema` heap used for dynamic allocations, including room geometry |

The displayed total is `2 * display lists + 2 * vertices/matrices + textures +
dynamic heap`. Other engine allocations and loaded level data also consume
RAM; this total is not a measurement of free memory. Entries accept 1–8192
KiB, but an accepted value does not guarantee the complete level fits in RAM.
The boot-only `-m`, `-mf`, `-ml`, and `-me` settings are not per-level budgets
and are not exposed here.

**Apply** commits the form to project metadata. **Revert edits** discards the
pending text, and **Use ROM defaults** removes the selected level's overrides.
Save Project, Create ROM, changing levels/tabs, and closing Level Settings
apply valid pending edits. Invalid entries remain focused for correction or
revert. Changes mark the project unsaved; Ctrl+Z in a field undoes text edits,
without changing geometry. These settings take effect in exported ROMs.

The named solo, multiplayer and Title entries each have their own allocation.
The game's existing stage/player-count selection remains unchanged. Shared
background geometry does not imply a shared allocation setting.

## Updating existing projects

Rebuild **both GUD and GEditor**, then rebase the project onto that GUD ROM.
The controls are disabled until the project's base ROM exposes `LMEM` slots.
Older projects can still open and export; rebasing enables this feature without
discarding their level edits.

GUD retains the existing allocation tokens and default values, but stores each
stage's string in its own reserved 64-byte slot. `STGT` keeps its 40-byte layout
and its pointer at offset 20. The optional `LMEM` manifest range identifies the
slots, with `flags = 64`; pointers are resolved through `CMAP`. This prevents
edits from overrunning a literal or changing another stage that originally had
the same string. Export replaces only edited slots and zeroes their unused
bytes. Repeated exports add no data, and leave `base.z64` intact.

Only changed fields are serialized as `memory = stageID|field|KiB` in `.gep`
version 4. Versions 2 and 3 remain readable, and projects without memory
overrides keep their existing version. Rebase merges each budget separately:
unedited fields adopt new defaults, project edits survive, identical edits
remove redundant overrides, and conflicting changes or missing edited
allocations stop publication of the rebased project.

## Verification

```sh
python3 tools/geditor/tests/level_memory/run.py
python3 tools/geditor/tests/project_rebase/run.py
python3 tools/geditor/tests/environment/run.py
python3 tools/geditor/tests/current_formats/run.py
make -C tools/geditor/src
```

After compiling the native `lv.o` and `gedmanifest.o`, run:

```sh
python3 tools/geditor/tests/level_memory/native.py
```

These checks cover form state, validation, persistence, exact ROM changes,
full exports, rebase/rollback, and native slot pointers/manifest sizes. Windows
interaction and the chosen budgets' adequacy still need in-game testing.
