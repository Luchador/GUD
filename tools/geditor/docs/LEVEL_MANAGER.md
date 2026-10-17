# Level Manager

Apply the patch, rebuild GUD and GEditor, then rebase an existing project onto
that rebuilt ROM (or create a project from it). Open Settings -> Project
Settings -> Level Manager, the first tab.

The left list contains all levels in the project's catalog. Drag a level onto
one of the 20 single-player boxes (4 rows, 5 columns) or 12 multiplayer boxes
(3 rows, 4 columns). Alternatively, click a box, select a level in the left list,
and use
**Assign selected level** in the inspector. Clicking a box only selects it; it
does not assign the highlighted level. Boxes are numbered from left to right,
top to bottom. Release outside a box or press Esc
to cancel a drag. Save Project saves assignments; export a ROM to play them.

The list includes Random (multiplayer), which may be placed in any MP box or
replaced entirely. Random chooses among the assigned, unlocked arenas that
support the current player count. At least one actual MP arena is required.
The same level can occupy multiple boxes. Title and catalog entries without
setup, background or stan resources cannot be assigned.

Assignments use the exact catalog entry: Facility and Facility (MP), for
example, load their respective setup resources. Choose the MP variant when
using a level's existing multiplayer setup. Assigning a level does not create
missing mission objectives, multiplayer spawns, or other mode-specific assets.
Existing solo briefings follow their level; levels without a solo briefing
show an empty briefing. Changed menu labels use the catalog level name.

Progression, save records, cheat requirements, bonus-slot rules, and mission/part
numbers remain attached to their original positions. Moving a level changes what
that position loads. Known MP arenas
retain their own picture and player-count limit; MP unlock requirements remain
attached to the destination box. Unrecognized arena IDs use the Random picture
and allow up to four players.

## Slot assets

Click a single-player or multiplayer box to open its settings in the third
column. The selected box stays highlighted. Settings belong to that box, so
assigning another level preserves its explicit asset choices.

| Setting | Single player | Multiplayer |
| --- | --- | --- |
| Menu icon | Project image, 68 x 44 | Project image, 68 x 44 |
| Briefing image | Upper and lower halves, each 128 x 64 | Not used |
| Menu name | Level-select label | Arena label |
| Mission name | Briefing mission heading | Not used |
| Part name | Briefing part heading | Not used |
| Menu briefing source | Assigned level, another solo mission, or No briefing | Not used |

Image choices include thumbnails and are filtered to native sizes/formats that
fit N64 texture memory. Icons accept IA8, I8, IA4, I4 or CI4. Briefing halves
accept IA4 or I4. Both require no mipmaps. Import your images through the Images
panel first. A briefing photograph is 128 x 128 overall, divided horizontally
into its upper and lower 128 x 64 images, matching the existing wallet model.
The game patches the wallet's native texture references when loading it, before
texture expansion; the project's original wallet model stays intact.

The three names are independent and allow up to 31 English ASCII characters.
Leave a name blank or choose Default for an image to use the existing default.
Stock single-player artwork follows the box; stock multiplayer artwork follows
the assigned arena. Default names follow the existing menu/level naming rules.
Custom names are literal text and do not replace shared language-bank strings.

The briefing-source selector chooses the menu briefing and its matching language
bank together. To change the selected source's text, open that level and use
**Level Settings -> Briefing**. Its authored menu briefing will be used by every
box referencing that source. This selection does not move the loaded mission's
watch briefing, objective conditions, or setup logic.

**Apply** updates the project in memory. Moving to another box, assigning a level,
closing Project Settings, or Save Project also applies the current form; invalid
values keep the form open. **Revert** discards the current form edits. **Defaults**
clears all custom assets for this box without changing its level assignment.
Use Save Project to write the settings, then export a ROM to play them.

## Persistence and compatibility

Projects store edited slots as `levelslot = index|levelID`, where indices 0-19
are single player and 20-31 are multiplayer. `-1` means Random. Stable level IDs
allow catalog rows to move during a rebase. Edited slots always favor the
project; untouched slots adopt the new ROM's defaults. A missing target level,
malformed table, or downgrade that cannot retain assignments blocks the rebase.

Asset overrides are stored per field as `levelasset = slot|field|value`.
Edited fields favor the project during rebase; untouched fields adopt the new
ROM's defaults. Clearing a custom base-ROM field to Default is also preserved. Image IDs remain
stable; missing/deleted images, incompatible artwork, missing briefing resources,
or a runtime downgrade block export/rebase instead of silently losing settings.

Older ROMs with LVSL but no LVAS still support level assignment. Asset controls
remain disabled until GUD is rebuilt with this update and the project is rebased.
Older ROMs with neither table remain readable/exportable without edits, but their Level
Manager boxes are disabled until rebased onto a ROM with the new runtime.
Keep using the updated GEditor after saving these assignments: older editor
versions do not understand the new project fields.

## Checks

```
python3 tools/geditor/tests/level_slots/run.py
python3 tools/geditor/tests/level_assets/run.py
python3 tools/geditor/tests/project_rebase/run.py
python3 tools/tests/level_menu/run.py
python3 tools/tests/level_identity/run.py
# After compiling the changed N64 objects:
python3 tools/tests/level_menu/check_abi.py build/u
```

These exercise native ROM byte writes, project persistence, full export and
rebase, campaign identity, exact SP/MP resources, briefings, labels and Random.
Asset checks cover the actual repository wallet, texture preloads, image validation,
per-field conflict handling, selection without assignment, and invalid draft retention.
The game still needs an emulator or hardware playtest with your project assets.
