# Level GLB export

With a level open, use **File → Export... → Export background** or
**Export stans**. Each opens a save dialog for a single `.glb` file. An item
is disabled when its corresponding geometry is unavailable.

Exports read the live level documents, so committed unsaved edits are included.
They do not save or change the project. Viewport hiding, layer visibility,
selection highlights, fog and stan opacity do not affect the output.

- Background exports contain both primary and secondary geometry, grouped
  into named room/layer meshes. Textures are embedded as PNGs in the GLB;
  UVs, vertex colors, supported alpha modes, wrapping and backface culling
  use the same material decoding as the editor preview. Objects and characters
  are separate setup assets and are not included.
- Stan exports triangulate every tile using the same fan as the viewport.
  They retain tile RGB colors and room grouping, and use opaque, double-sided
  materials so floors and ladders can be viewed from either side.
- Both exports use the same Y-up coordinates in meters (gameplay world units
  divided by 100), so they align when imported together. Room origins and level
  scaling are already applied; editor unit preferences do not change the output.

Standard unlit glTF materials preserve the level's baked colors. Native effects
such as fog, detail-texture combiners, decal depth bias and camera-dependent
environment mapping are not reproduced by standard glTF materials. Relevant
native texture/render flags remain in material extras.

The GLB writer uses the existing model material/PNG export path, writes aligned
JSON/BIN chunks and replaces the selected destination only after a successful
write. Existing `.gltf` model exports retain their source identities and format.

Run `python3 tools/geditor/tests/level_export/run.py` for synthetic output and
dialog checks. Optional `--bg file.seg --stan file.stan` adds full native-level
geometry checks; texture lookup/Windows PNG encoding use fixture images in the
host tests. `--output directory` retains test artifacts for inspection.
