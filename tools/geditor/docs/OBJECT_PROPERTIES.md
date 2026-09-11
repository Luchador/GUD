# Shared object inspector

Select a setup object and open the existing **Properties** tab. The shared
inspector displays its readable type, an editable **Model** dropdown, an editable
**Health** field, and read-only object/model IDs, placement reference and extra
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
- Save Project writes the edited setup resource used by ROM creation. Existing
  projects work; no extraction or GUD rebuild is needed to use the inspector.

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

Add future ammo, key and door sections according to object type, with matching
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
with Win32 controls stubbed. Windows visual testing is still needed for panel
layout, scrolling, dropdown interaction and model previews.
