# Opaque texture batching

GEditor batches compatible opaque background faces when compiling edited BG
geometry and when creating a ROM, including rooms not opened during the session.
The export pass replaces primary display lists in their existing space: it does
not grow the BG file, move portal addresses, or rewrite secondary geometry.

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

Synthetic interleaved fixtures reduce six BG texture selections to three and
five static-prop selections to two. Native Depot, Facility, Archives, Bunker,
Egypt and Silo backgrounds remain unchanged under these checks. These are native
command/geometry regressions, not GPU timing or an emulator frame-rate benchmark.
