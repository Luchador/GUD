# Background face decals

Select one or more background faces and set **Decal** to **On** or **Off**
in Face Properties, below Backface culling. Mixed means the selected faces
have different settings; choosing On or Off applies to all of them.

On uses the N64's coplanar decal depth mode, allowing a face to draw on its
supporting surface without moving the vertices. It retains depth testing, so
closer geometry still hides it, and does not replace the supporting depth.
GEditor's existing decal preview uses a small depth bias to approximate this.

The supporting surface must draw first and write depth. Normally keep the
wall/floor in Primary and put its decal in Secondary using the Layer control.
This setting does not move faces between layers or reorder their draw calls.
Overlapping decals in the same layer retain their authored order.

Transparency is independent: a decal can be Opaque, Cutout, or Translucent.
Changing transparency, including returning to Auto, retains the Decal choice.
Off returns to the normal opaque/cutout or translucent depth mode. It is not
an exact restoration of unusual preexisting intersection/coverage modes; Undo
restores the original edit exactly.

The property is read-only for inherited/custom render pipelines or disabled
depth testing. Existing native decals are recognized without conversion.
Changes participate in undo/redo and are stored as ordinary native render
commands during project save and ROM export. No GUD runtime update or project
rebase is needed. Decals remain barriers to opaque texture batching and are
not converted to ordinary surfaces by the one-cycle optimizer.

Validation: `python3 tools/geditor/tests/bg_transparency/run.py` covers native
mode words, mixed selection, isolated triangle edits, independent transparency,
save/reload, undo/redo, and returning to normal surfaces.
