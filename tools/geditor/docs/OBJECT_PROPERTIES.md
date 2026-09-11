# Shared object inspector

Select a setup object and open the existing **Properties** tab. The shared
inspector displays its readable type, an editable **Model** dropdown, an editable
**Health** field, and read-only object index, placement reference and extra
scale. Transforms stay in the transform panel; object flags stay in **Flags**.

- Model choices use the prop model catalog and the models available in the
  project's `models/objects` folder. Choosing one updates the viewport immediately.
  It changes the existing object's appearance, preserving its type, flags,
  placement, scale and specialized fields.
- Health accepts decimal values. Enter or leaving the field commits an edit;
  Escape cancels typing. Invalid input displays a message without changing the
  setup. Values are rounded to the setup's 1/65536 precision.
- Each committed change creates one Undo/Redo step and marks the setup unsaved.
  Unchanged values create no step. Ctrl+Z first undoes pending text edits while
  the Health field has text to undo, then uses the editor's history.
- Save Project writes the edited setup resource used by ROM creation. Projects
  must use the matching current GUD ROM and setup format.

Health is the authored destruction threshold, stored at ObjectRecord offset
`0x74` (`damage`) as signed 16.16 fixed point. Despite its name, `maxdamage` at
`0x70` is accumulated damage during gameplay and is not the field to edit.
The input range is 0 through 32767.99998474121. An object's type and flags can
make it indestructible regardless of its Health setting. Characters have a
separate GuardRecord layout and are not edited by this inspector.

Model IDs at `0x04` are prop catalog IDs, not first-person gun or character
model IDs. The inspector verifies that the chosen project model loads, rolling
back if it fails. Specialized objects such as doors and monitors may require
particular model parts; this first pass does not validate those requirements.

## Keys and ammo

These sections appear only for the selected record type, in the Properties tab.
Enter or leaving an input applies it; Escape cancels typing. All changes use
the same setup save, ROM export and Undo/Redo path as the shared fields.

- **Keys (PROPDEF_KEY):** edit the hexadecimal unlock mask or toggle its 32
  checkboxes. Checkbox 1 is bit `0x00000001`; checkbox 32 is `0x80000000`.
  These are flag positions, not door IDs. The player's collected keys contribute
  flags together, and a door requires every bit in its own required mask.
  Zero is valid but contributes no unlock flags. Edit the matching required
  flags by selecting a door.
- **Single-ammo pickups (PROPDEF_MAGAZINE):** select the ammo type. Quantity is
  determined by `get_ammo_in_magazine`, with a solo multiplier where applicable;
  there is no authored quantity field in this record.
- **Ammo crates (PROPDEF_AMMO):** choose an ammo slot, then edit its base
  quantity (0–65535). Choosing a slot only changes which field is displayed.
  Set its quantity to zero to remove that type from the contents; other slots
  are preserved. The Model field controls the crate itself.

Contents shows each nonzero ammo type's pickup amount on Agent, Secret Agent,
00 Agent and 007 Mode. Their current multipliers are 2, 1.5, 1 and 1, respectively.
Fractional results truncate just as in the game: a base quantity of 5 gives
10 / 7 / 5 / 5. These are the amounts offered before inventory limits; the actual
increase depends on how much the player already carries.

There is one 9mm ammo type and one 9mm crate slot.
The preview updates on committed edits and Undo/Redo without modifying setup data.

Multiplayer Contents shows unmultiplied setup quantities instead of solo difficulties.
Multiplayer setup can override a crate slot's quantity from the weapon set
associated with the preceding weapon placement, and can omit the crate when
that weapon slot supplies no ammo. The inspector preserves this game behavior
and displays a note for multiplayer setups.

Native fields: keys and single-ammo types use the 32-bit word at `0x80`.
Multi-ammo slots occupy 12 words starting at `0x80`, in ammo-ID order 1–12;
each contains a 16-bit model followed by a 16-bit quantity. Only quantity is
editable; the original model half remains intact through edits, saving and ROM
creation. `src/ammoconstants.h` shares the AMMOTYPE enum and solo ammo multiplier
constants between the game and editor. Ammo IDs are compact: 9mm is 1, rifle
is 2, and Golden Gun ammo is 12. Rebuild GUD and create a new project after
this refactor; manifest version 3 rejects ROMs with the old thirteen-slot layout.
Do not replace an old project's base ROM or copy old setup files into a new project.

## Doors

Selecting a `PROPDEF_DOOR` shows these additional controls in **Properties**:

| Setting | Meaning |
| --- | --- |
| Door movement | Sliding, three folding variants, vertical, swinging, eye, iris, fall-away or Aztec chair. Special types require compatible model parts. |
| Open travel | Percentage of the fitted door width (height for vertical/fall-away), degrees for swinging/chair, or animation units for eye/iris. |
| Collision clearance | Travel at which the door stops blocking passage. Uses the same units as open travel. Values above full travel keep collision active while open. |
| Maximum speed | Travel units per second. |
| Acceleration / deceleration | Travel units per second squared. Both must round to a positive native value. |
| Auto-close delay | Seconds after reaching fully open. Zero closes without waiting; it does not disable automatic closing. The general Start open flag can hold a door open. |
| Door sounds | One preset controls opening, moving and closing sounds. Silent plus the 17 implemented presets are available. |
| Door flags | Extended vertical collision, windowed distance transparency, clipping the sliding mesh to its bounds, and mirroring front/back. |
| Required keys | A hexadecimal mask or 32 checkboxes. Zero needs no key. The player needs every checked bit, potentially from multiple keys. |

Enter or leaving a field commits; Escape cancels typing. Changes use the existing
setup save, ROM creation, unsaved-state and Undo/Redo system. The viewport shows
the static model; it does not simulate door motion or preview door sounds.
General door flags such as Start open, Two way and Interlock remain in **Flags**.
Linked-door references and window fade distances are preserved but not edited
in this pass. Clipping needs compatible model geometry; changing movement type
retains the native motion values and updates their displayed units.

Door enums now live in `src/doorconstants.h`, shared by GUD and GEditor. This move
changes no IDs or setup layouts. Rebuild GEditor; existing current-format projects
and ROMs work without regeneration.

Native encoding follows `DoorRecord` and `setupDoor`:

| Offsets | Encoding |
| --- | --- |
| `0x84`–`0x94` | Travel, clearance, acceleration, deceleration, speed: signed 16.16 fixed point. Rates use 60 Hz game ticks. |
| `0x98` / `0x9a` | 16-bit door flags / movement type. Each setter preserves the adjacent half-word. |
| `0x9c` | 32-bit required-key mask. |
| `0xa0` | Close delay in 60 Hz ticks, rounded to the nearest tick. Authored edits are limited to the signed timer range. |
| `0xa4` | 32-bit sound preset. |

Unknown flag bits and unsupported existing type/sound values are preserved.
Runtime pointers, current travel, speed, state and collision caches are untouched.

## Extending the inspector

`SetupFileGetObjectProperties` decodes a selected record on demand.
`SetupFileSetObjectProperty` owns native layout, encoding, subtype validation,
range checks and no-op detection. It changes only the requested bytes and any
corresponding cached SetupObject field. The raw setup remains authoritative.

`objectproperties.c` owns controls and uncommitted text. It sends a synchronous
property request containing the object index, source offset and type. The main
frame checks the current selection, wraps the setter in an existing setup-history
transaction, refreshes geometry when needed, and restores the prior state on
failure. No control writes directly into a setup record.

Add future specialized sections according to object type, with matching
validated property IDs/setters. Keep runtime state and derived fields out of the
editable controls. Fields needing richer values can extend the request payload
without moving serialization or history logic into the panel.

## Validation

From the repository root:

```sh
python3 tools/geditor/tests/object_properties/run.py
make -C tools/geditor/src
```

The native checks cover all 21 parsed ObjectRecord types, exact preservation of
unrelated bytes, save/reload, quantization, invalid/stale requests, history and
rollback. Input checks exercise production parsing and keyboard/commit logic
with Win32 controls stubbed. They also cover all ammo slots, zero/full quantities,
four-difficulty previews, per-slot rounding, the single 9mm slot, complete summaries
for full crates, high key bits and rejection of fields belonging to another
object type. Door checks cover every implemented type/sound, exact native field
bytes, key masks, unknown flag preservation, unit conversions, timer rounding,
invalid acceleration and per-field save/history transactions. Windows visual
testing is still needed for panel
layout, scrolling, dropdown interaction and model previews.

After building GUD, `python3 tools/geditor/tests/ammo_layout/run.py` checks the
compiled N64 ammo tables and setup command boundaries. An optional second build
directory compares quantities, command references and weapon stats against the
pre-refactor layout.
