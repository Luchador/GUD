# Editing models in Blender

The Model Editor now has **Export Model...** and **Import Model...** buttons.
Import replaces the selected model throughout the project. Save Project saves
model replacements, and Create ROM includes them. Importing marks the project
as unsaved; closing the Model Editor does not discard its changes.

1. Select a character, item, or prop in Tools > Model Editor.
2. Click **Export Model...** and save the glTF somewhere convenient. This works
   with existing projects that have `base.z64`; no new project is required.
3. Import that file into Blender. Leave **Merge Vertices** disabled.
4. Move existing vertices in Edit Mode and adjust their UVs in the UV Editor.
   Paint the existing color attribute in Vertex Paint to change vertex colors.
   You can also delete faces. To remove texture usage from remaining faces,
   disconnect the material's image texture from Base Color. Keep the color attribute.
   Assigning another existing `GUD Image` texture of the same dimensions is also
   supported. Keep the image names so GEditor can identify their ROM texture IDs.
5. Export from Blender as glTF Binary (`.glb`) or glTF Separate (`.gltf`). Enable
   **Include > Custom Properties**, **Data > Mesh > Attributes**, **UVs**, and
   **Vertex Colors**. Export the complete model, including its other parts and
   LODs. Do not enable mesh compression. Keep separate-format `.bin` files
   beside their `.gltf` file.
6. Select the same model in GEditor and click **Import Model...**. Review the
   viewport and the before/after counts in the footer, then **Save Project**.
7. Use **Tools > Create ROM** and test the result in-game.

New projects' extracted model glTFs also contain the import identities. Using
Export Model remains useful after an import because it exports the current
revision. An older export cannot overwrite a newer imported revision: export
again before making another round of edits.

## What is supported

Import supports moving existing vertices and UVs, painting vertex colors,
face deletion, removing
texture assignments, and reassigning existing textures of the same dimensions.
It validates triangle identities and rejects added/duplicated faces, changed
triangle connections or index winding. Do not merge
vertices, retriangulate, apply a decimation modifier, or remove `_GUD_VERTEX`.

Native positions use signed 16-bit integers in each vertex's original joint
space. The importer converts the exported rest pose back to that space and
rounds positions to the nearest integer. UVs are rounded to 1/32 texel using
the project's texture dimensions; negative and repeating UVs outside 0–1 are
allowed. Values outside the native signed 16-bit range are rejected, rather
than clamped. Review the imported preview to see the quantized result.

Exported triangles have separate corners to retain their source identities.
When several corners reference one native vertex, move **all copies together**,
including copies in another part or LOD. Their positions, textured UVs and
native color values must agree after rounding. Use X-Ray selection for coincident positions and select
all corresponding UV corners. Do not weld them: that loses their individual
`_GUD_VERTEX` identities. Existing native UV seams remain editable, but creating
a new seam by separating UVs that share a native vertex is not supported.
Paint all shared color corners consistently, too; creating a new color seam by
splitting a native vertex is not supported. Conflicting edits are rejected
without replacing the current model.

Color-based geometry accepts RGB edits and, where the authored material uses
vertex alpha, alpha edits. glTF `COLOR_0` may contain RGB or RGBA floats or
normalized unsigned bytes/shorts. Components must be finite values in 0–1 and
are rounded to the N64's 8-bit channels. RGB-only attributes imply alpha 1.
This edits the color attribute, not the material's Base Color factor.

The importer retains the native combiner and blending settings. Lowering
vertex alpha does not automatically turn an opaque part into a translucent
one. Where opacity comes from a material constant or texture instead of vertex
alpha, keep the exported alpha unchanged; the importer preserves the hidden
native alpha bytes. RGB can still be painted on these color-based parts.

When native lighting is enabled, the vertex's RGB bytes store signed normals.
Those parts keep their RGB unchanged so painting cannot corrupt their normals.
Their alpha remains editable if the material uses vertex alpha. Normal editing
and conversion between lit and color-based geometry are not supported here.

Reflection-mapped parts generate UVs in-game. Their positions can move, but
their exported UVs must remain unchanged. Their authored normals are retained;
this import does not recalculate model lighting or accept new normals.

Exports separate native draw parts/material groups into objects. Distant
character LOD objects are labeled **(distant LOD)**. Edit those too when a change
should also appear at a distance; their topology can differ from the closest
LOD. The GEditor character preview continues to show the closest LOD. The
import footer counts all exported LODs, while the viewport counts displayed
geometry.

The compiler retains the native skeleton, per-vertex joint associations,
attachment points, switches, collision/bounding data and dynamic effects such
as muzzle flashes. Position/UV/color-only edits update the original native vertex
records and preserve all rendering commands byte-for-byte. Face deletion or
material changes rebuild triangle/material commands and omit vertex loads
unused by surviving faces. Vertices shared with preserved dynamic effects
cannot be modified through the ordinary mesh export.

Moving or removing visible geometry does not reshape collision/culling bounds
or move attachment points. Keep edits within the authored bounds; expanding a
model beyond them may require separate changes to its game metadata.

For `Pjungle3_treeZ`, export the current model, select every copy of the vine
edge vertices you want to bring inward, and adjust their corresponding UVs to
keep the visible vine aligned. Preserve the existing triangles and attributes.
Import, save, and test the new ROM on N64 to measure the effect on overdraw.

Unused texture images remain in the project images folder, GEditor's image
browser and the ROM. Removing a model's texture usage does not delete any
shared texture asset. Edited glTF exports embed the textures they still use.

## Saved data

`models/native/<name>.gmodel` is the authoritative saved native replacement.
It contains a compiled model plus fingerprints identifying its base-ROM source
and checking the file contents. The matching model `.gltf` is refreshed on save
as an editable export. Keep the `.gmodel` files with the project.

The ROM builder validates saved replacements, updates resource offsets if a
model grows, and preserves all other resources and the original `base.z64`.
The existing project save-before-ROM-build workflow saves pending model imports.

## Regression tests

Run `python3 tools/geditor/tests/model_vertex_uv/run.py` with a host C compiler.
The suite exercises the native compiler, glTF/GLB importer, saved overrides and
the ROM replacement reader with Jungle trees, a segment-4-addressed book model,
and a mixed-joint vertex-cache fixture. It checks deformation, UV quantization,
shared-vertex conflicts, range errors and compatibility with face deletion.
Color tests cover native RGB/RGBA writeback, color accessor encodings, missing
or invalid colors, normal protection at vertex-load time, material-controlled
alpha, shared alpha across different materials, and saved replacement reads.
Texture/ROM directory lookup and Windows file APIs are stubbed; Windows UI,
PNG encoding and full ROM execution still require the normal application.
