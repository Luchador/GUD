# Object group transforms

Select multiple placed models, then use W/E/S or the transform fields.
The pivot averages the centers of the selected models' visible bounding boxes;
each model contributes equally. One completed operation creates one undo step.
Escape restores the preview, and a placement failure rolls back the whole group.

Translation moves the selection together. Rotation turns its positions and
orientations around the shared pivot in world space. Numeric group rotation
fields specify a rotation to apply and return to zero after the edit.

Group scaling changes spacing and dimensions along the world gizmo axes.
Each prop's pad-axis factors are computed from the lengths of those axes after
the world scale. This accounts for door axis permutations and rotated props:
world-aligned members follow the gizmo exactly, regardless of pad orientation.
Oblique members keep their orientation and use the resulting axis lengths,
because native orthogonal placement pads cannot store shear. The center scale
handle applies one factor to all three axes. The preview and saved placement
use the same rules; ordinary pads become bound pads where needed.

Characters retain their existing grounding rules, support only Y rotation,
and cannot be scaled. A mixed selection containing characters follows those
rotation/scale limits. Door shadows continue to follow their owning doors.
Shift-drag duplication currently requires a single object or character.

The Level panel sorts names without changing project indices or campaign order.

Checks:

- `python3 tools/geditor/tests/object_group/run.py`: native transforms, differently
  oriented props and doors, shared pads, saved files, undo/redo and rollback.
- `python3 tools/geditor/tests/uniform_scale/run.py`: group drag previews,
  cancellation, selection masks, shared pivot and existing single-selection tools.
