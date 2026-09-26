# Reverse Edge

In Edge selection mode, select one background edge and press **T**, or choose
**Edge > Reverse Edge**. Right-clicking a background edge also offers
**Reverse Edge** for that edge. The menus disable it when the edge cannot be
reversed. Ctrl+T still opens the UV Editor; typing in input fields is unaffected.

The edge must be shared by exactly two triangles forming a convex quad, with
the same room, layer, material and compatible draw state. The tool replaces
their common diagonal with the opposite diagonal. It does not join coincident
but disconnected vertex records or cross a material/UV split.

The four native vertices retain their coordinates, UVs and colors. Face IDs,
materials, winding and vertex/triangle counts are retained. Interpolation
follows the new triangulation; a non-flat quad's surface can therefore change.
Boundary seam guides stay on their edges, and a guide on the old diagonal moves
to the new diagonal.

The new diagonal stays selected, so T flips it back. Each operation is one undo
step and is saved in the BG file used by ROM export. This tool edits background
triangles; it does not alter stan tiles or placed models.

Run `python3 tools/geditor/tests/reverse_edge/run.py` for native triangulation,
UV/color/seam preservation, save/reload, ROM batch validation, undo/redo,
transaction rollback, hotkey scope and viewport context-menu checks.
