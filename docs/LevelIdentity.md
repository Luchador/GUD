# Level identity and campaign order

These values have separate meanings:

| Value | Source | Meaning |
| --- | --- | --- |
| `LEVELID_*` | `src/levelids.h` | Persistent identity used by gameplay, assets, projects, environments and demo headers. |
| `SP_LEVEL_*` / `LEVEL_SOLO_SLOT` | `src/levelids.h` | Persistent EEPROM slot for completion times and cheat unlocks. |
| Catalog slot | Generated from `src/game/leveltable.inc` | Storage position only. Never use it to identify a level in gameplay. |
| Campaign order | Playable rows of `mission_folder_setup_entries` | Menu position and progression order; header rows do not consume positions. |
| `number_text` | Mission header or part row | Displayed mission/part number. It has no identity or progression meaning. |

Existing level IDs and save slots retain their original numeric values. Their
explicit assignments prevent accidental renumbering when declarations move.
`LEVELID_MAX` is the old placeholder's identity, not a count or an upper bound.
The environment offsets remain a serialized format contract, not table order.

## Level catalog

`leveltable.inc` generates the level entries, allocation slots and count from
one list. Moving a row moves its budget with it. Look up levels with
`lvFindLevelInfo` / `lvFindStageInfo`; background rendering retains the resolved
record and uses a `LEVELID` for special portal/culling behavior.

GEditor shares the lightweight `levelids.h` definitions for briefing/text-bank
selection, Title recognition and environment variant offsets. Retired-stage
migration IDs deliberately remain fixed historical numbers.

## Campaign

Each playable row explicitly identifies its level, save slot, photograph,
cheat target times, minimum difficulty and bonus status. Header/part number
strings are presentation only. The `campaignGet*` helpers distinguish a level
ID, a save slot, a campaign position and a raw row index; callers must choose
the appropriate lookup.

Moving rows changes menu order without moving saved records or cheat unlocks.
The menu moves photograph geometry to the corresponding campaign positions
while retaining each photograph's texture. Header rows still group subsequent
parts and supply their displayed mission number/title.

Main missions advance to the next main row. The last main mission leads to
Cuba's credits; bonus missions return to mission selection. Main progression
ignores bonus rows, and bonus unlocks require the main campaign plus preceding
bonus missions. Unlock button codes use those same prerequisites, and new
folders start on the first main mission even if a bonus row appears first.
The original campaign preserves its previous unlock behavior.

This is preparation for an editor UI, not the UI itself. The existing 20-save-
slot format and 5-by-4 mission menu remain fixed capacities. Adding more
missions, changing persistent IDs, or importing recordings/projects with other
IDs requires a separate format/migration change. Reordering the existing
campaign or changing displayed numbers requires no ID migration.

## Verification

`python3 tools/tests/level_identity/run.py` checks both original and reversed
catalogs, compares 163,840 original-order unlock decisions and 40,960 unlock
button-code checks against the prior rules, and exercises moved missions, bonus rows, display labels, save slots,
photographs, cheat times and level-specific portal ranges. Controlled saves
exercise the unlock logic even while the game's existing temporary
always-completed override is enabled.
