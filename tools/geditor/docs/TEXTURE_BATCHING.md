# Background and static-prop batching

GEditor optimizes background batches automatically on Save Project and Create
ROM, including unchanged files and levels not opened during the session. The
compiler works on serialization copies; live vertex/face IDs, selections, seams
and undo data stay intact.

Static batches collect up to sixteen distinct vertices, regardless of their
original index spacing. Each batch is written as one contiguous G_VTX block,
which also satisfies the CPU bullet tests' last-load bounds. Byte-identical
XYZ/flag/UV/RGBA records can share a cache slot. Different UVs, colors, alpha or
flags remain separate. Dense blocks use first-reference order so repeated
save/reload does not shuffle vertex storage or accumulate copies.

Vertices referenced by shootable lights, animated water, untextured faces with
inherited hit textures, or unsupported state retain their source sharing.
These mutable aliases must not merge with otherwise identical static records.
The light texture IDs mirror `check_if_imageID_is_light` in `lightfixture.c`.

Redundant other-mode, geometry, environment-color and editor-policy writes are
removed only when their relevant bits are already known. First writes and real
state transitions remain. Primitive-color writes, pipe syncs, unknown commands,
texture-loader side effects and alpha-scope restoration are preserved.
Secondary and decal faces can share vertex loads across material-only groups,
while keeping their triangle order and authored transparency.

Automatic optimization of existing files requires known flat room streams and
accepts a rebuild only if texture selections, vertex loads, transformed vertex
entries and total BG size do not increase. Unsupported streams or rejected
rebuilds use the existing in-place opaque batching and orphan cleanup instead.
Normal resource compaction preserves room/portal/visibility data and empty-room
bounds. No texture is automatically changed from Blend to Cutout by this pass.

New static props use batching on import/reimport. Once images have been assigned
to a new prop's material slots, reimport its source model to batch those bindings;
the named material assignments are retained. Character and existing native model
compilers are unchanged. Translucent prop faces retain their original order.

Batch keys include complete texture commands, detail images, wrapping, texture
scale, combiners and culling. Room boundaries and authored state transitions
remain barriers. Blended, cutout, decal, generated-UV and unknown-state BG runs
are excluded. Untextured faces, vertex-alpha scopes and special water bindings
also remain barriers. Each reordered run ends with its original material to
preserve inherited texture state used by bullet hits and breakable lights.

The compiler compares the original and grouped output before choosing. It keeps
the original when grouping would increase stream size or vertex loads; BG checks
also count transformed vertices and appended vertex bytes. Live editor geometry,
face identities, seams and undo data are not reordered. Prop material/UV metadata
is remapped with the compiled faces.

Checks:

```sh
python3 tools/geditor/tests/texture_batching/run.py
python3 tools/geditor/tests/new_props/run.py
python3 tools/geditor/tests/unused_vertices/run.py
python3 tools/geditor/tests/face_alpha/run.py
python3 tools/geditor/tests/project_rebase/run.py
```

The BG checks cover sparse editor indices, every native vertex attribute,
cross-layer light/water aliases, partial state writes, transparent/decal order,
immutable inputs and repeated export. Depot, Facility, Archives, Bunker, Egypt,
Silo, Dam and Jungle provide native fixtures. Additional uncompressed `.seg`
files can be passed to `texture_batching/run.py`.

For the audited edited Cradle BG, all 2,642 triangles survive while vertex loads
fall from 498 to 385, loaded vertex entries from 5,493 to 5,209, triangle commands
from 815 to 735, and file size from 118,352 to 109,584 bytes. These are whole-file
command/geometry measurements; in-game frame-time gains require measurement.
