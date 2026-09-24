# Face attributes and layers

**Correct Face Attributes** is a toolbar checkbox, enabled when GEditor starts.
With it enabled, committing a background vertex or edge move also corrects the
affected UVs. This includes gizmo moves and typed positions.
The position and UV changes share one undo/redo entry and are saved normally.
Turn it off to retain the original UV coordinates while moving geometry.

Correction extends the original triangle's mapping for planar moves. For a
fold, it unfolds the changed triangle around the edge whose length and direction
are unchanged, preserving texture density as the extrusion tool does. Shared
vertices combine adjacent triangles' estimates, weighted by original area;
independently authored seam vertices retain their separate mappings. Correction
does not split vertices or change their colors. Degenerate source triangles
cannot supply a mapping and are skipped. Moving an entire triangle together
supplies no correction. Conflicting mappings around a shared vertex are a best
fit, so the UV editor remains useful for final adjustments.

UV correction is applied when the move finishes. Moving whole faces, rotating,
scaling, and editing stan tiles, portals, pads, or setup objects use their existing
behavior. Extrusion retains its own UV generation. If corrected UVs exceed the
native coordinate range, the move is rolled back; reduce the displacement or
turn off the checkbox.

In Face mode, the properties panel now has a **Layer** dropdown with **Primary**
and **Secondary** choices. **Mixed** indicates a selection spanning both layers;
choosing a layer applies to every selected face, including selections across
rooms. Choosing Mixed leaves the current assignments intact.

Layer changes retain face identities, shared vertices, UVs, colors, room
assignments, detail textures, and explicitly authored rendering state. Each
affected room receives the source display-list state needed by the moved faces.
Inherited defaults may differ in the new draw pass. A vanilla face displayed as
untextured without an assigned image is made explicitly untextured, preventing
it from accidentally inheriting the destination's last image. Layer changes do
not choose a new transparency preset; use the Transparency control for that.

The selection follows the moved faces. Layer edits support undo/redo, project
saving, and ROM creation; choosing the existing layer creates no history entry.

Validation: `python3 tools/geditor/tests/face_attributes/run.py` exercises UV
correction and seams, native ranges, layer transfers and material preservation,
allocation failures, undo/redo, save/reload, and Depot geometry. Toolbar layout
and room-property regressions use the existing `geometry_toolbar` and
`face_rooms` tests.
