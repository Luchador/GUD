# Selecting models and editing flags together

- **Shift-click** a viewport object or character to add it to the selection.
  Clicking an already selected model with Shift keeps it selected.
- **Ctrl-click** removes just that model. An ordinary click selects one model.
  These controls work in face, edge and vertex selection modes.
- **Select → Select Similar**, or **Shift+S**, selects placed models matching
  the selection. Objects match by object model ID; characters match by body
  model ID, regardless of head or equipment. With multiple seeds, the command
  selects the union of their matches. Object and character IDs are separate;
  deleted records and carried/contained object records are excluded.
- Selected visible models have individual white outlines. **Z** frames the
  whole selection. Selection changes participate in Undo/Redo.

Open the right panel's **Flags** tab to edit a selection. Object PROPFLAG and
PROPFLAG2 sections apply to the selected objects; the character section applies
only to the selected characters' saved 16-bit setup flags. Section headings show
how many models each section affects. Character flags are not runtime AI flags.
Type-specific object options appear only when all selected objects share that
record type. Hover over a checkbox for its native name and description.

A checked box means every applicable model has the bit set. An empty box means
none have it; a shaded box means the values differ. Clicking a shaded or empty
box enables the bit for every applicable model. Clicking a checked box clears
it. Other bits, records and properties are preserved. Each checkbox edit is one
undoable transaction, including its geometry rebuild. Failure rolls back the
whole edit. Flags persist in project saves and ROM setup data.

Flags that hide objects retain their selection and editable flag values, so a
mode-exclusion bit can be cleared again. Undo also restores hidden models.
Other model properties and transforms require a single selected model; the
inspector explains this when a group is selected. Shift+S stays inactive while
flying the camera, typing in text/combo controls, or using another editor window.

Validation:

```sh
python3 tools/geditor/tests/object_flags/run.py
python3 tools/geditor/tests/selection_history/run.py
python3 tools/geditor/tests/bg_selection/run.py
python3 tools/geditor/tests/zoom_selected/run.py
python3 tools/geditor/tests/box_selection/run.py
make -C tools/geditor/src
```

Native checks run production matching, flag mutation, selection snapshots,
shortcut routing and framing logic under ASan/UBSan. They cover mixed values,
separate model namespaces, every saved character bit, preservation of unrelated
bytes, save/reload, allocation failures, transaction rollback and undo/redo.
Interactive highlighting, focus and checkbox appearance require Windows testing.
