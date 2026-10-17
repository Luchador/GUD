# Level Manager

Apply the patch, rebuild GUD and GEditor, then rebase an existing project onto
that rebuilt ROM (or create a project from it). Open Settings -> Project
Settings -> Level Manager, the first tab.

The left list contains all levels in the project's catalog. Drag a level onto
one of the 20 single-player boxes (4 rows, 5 columns) or 12 multiplayer boxes
(3 rows, 4 columns). Alternatively, select a level and click a box. Boxes are
numbered from left to right, top to bottom. Release outside a box or press Esc
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

Progression, save records, cheat requirements, bonus-slot rules, chapter
headings and single-player menu pictures remain attached to their original
positions. Moving a level changes what that position loads. Known MP arenas
retain their own picture and player-count limit; MP unlock requirements remain
attached to the destination box. Unrecognized arena IDs use the Random picture
and allow up to four players. Custom menu artwork is not part of this tool.

## Persistence and compatibility

Projects store edited slots as `levelslot = index|levelID`, where indices 0-19
are single player and 20-31 are multiplayer. `-1` means Random. Stable level IDs
allow catalog rows to move during a rebase. Edited slots always favor the
project; untouched slots adopt the new ROM's defaults. A missing target level,
malformed table, or downgrade that cannot retain assignments blocks the rebase.

Older ROMs remain readable and exportable with no slot edits, but their Level
Manager boxes are disabled until rebased onto a ROM with the new runtime.
Keep using the updated GEditor after saving these assignments: older editor
versions do not understand the new project fields.

## Checks

```
python3 tools/geditor/tests/level_slots/run.py
python3 tools/geditor/tests/project_rebase/run.py
python3 tools/tests/level_menu/run.py
python3 tools/tests/level_identity/run.py
# After compiling the changed N64 objects:
python3 tools/tests/level_menu/check_abi.py build/u
```

These exercise native ROM byte writes, project persistence, full export and
rebase, campaign identity, exact SP/MP resources, briefings, labels and Random.
The game still needs an emulator or hardware playtest with your project assets.
