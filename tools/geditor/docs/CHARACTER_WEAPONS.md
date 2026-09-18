# Character hand weapons

Select a character in the viewport, then use **Right-hand weapon** and
**Left-hand weapon** in the **Properties** tab. Each list includes **None**.
Changes immediately rebuild the character and held-weapon preview, retain the
selection, and create one undo step. Save Project and Create ROM include the
assignments.

These controls set the visible weapons held when the setup creates the
character. The native item type and its corresponding prop model change
together. Existing instance scale, weapon flags and difficulty exclusions are
preserved. If a hand has different items in multiple setup records, it shows
**Multiple setup variants**; choosing a weapon updates those variants together.
The preview uses the first visible assignment, as before.

**None** disables all visible assignments for the chosen hand. Re-equipping
reuses those cleared records and restores their original difficulty exclusions.
Concealed inventory, hats, the other hand and other characters are unaffected.
Weapons supplied later by Action Blocks can still appear in the game.

Existing item types outside the dropdown catalog appear as **Item N (current)**
and remain intact until replaced or cleared. Choosing the current item does not
reset a custom model or scale. An unavailable weapon model or failed scene
rebuild rolls the edit back.

## Native representation

Each hand uses `PROPDEF_COLLECTABLE` records with `PROPFLAG_ASSIGNEDTOCHR` and
the character ID in the pad field; the left hand adds
`PROPFLAG_WEAPON_LEFTHANDED`. A new record is appended after the owning guard,
without renumbering existing setup commands or their relative references.
Existing variants are edited in place. The setup is compacted atomically.

Cleared weapons keep their command slots and all five mode-exclusion bits.
A `GECW` marker and the original flags2 occupy two unused runtime matrix words
at offsets 0x20 and 0x24. Prop/model pointers remain untouched. Restoring these
records clears the marker words; the game builds the equipped weapon matrix.
This prevents repeated None/equip cycles from accumulating setup records.
Unrelated deleted records are never revived by these controls.

The catalog in `weaponchoices.h` is generated from the canonical `ITEM_IDS` and
`PROP` enums by `scripts/generate_weapon_choices.py`. The Windows make target
regenerates it when the enums or script change.

Validation: `python3 tools/geditor/tests/character_weapons/run.py` checks both
hands, mixed difficulty variants, concealed/deleted inventory isolation,
command references, native save/reload, undo/redo, no-op/stale requests, bounded
record reuse, allocation failures, dropdown payloads and frame rollback under
AddressSanitizer/UndefinedBehaviorSanitizer. Window and rendering calls are
stubbed; interactive Windows and in-game validation remain manual.
