# Editing models in Blender

The Model Editor now has **Export Model...** and **Import Model...** buttons.
Import replaces the selected model throughout the project. Save Project saves
model replacements, and Create ROM includes them. Importing marks the project
as unsaved; closing the Model Editor does not discard its changes.

1. Select a character, item, or prop in Tools > Model Editor.
2. Click **Export Model...** and save the glTF somewhere convenient. This works
   with existing projects that have `base.z64`; no new project is required.
3. Import that file into Blender. Leave **Merge Vertices** disabled.
4. Delete faces in Edit Mode. To remove texture usage from remaining faces,
   disconnect the material's image texture from Base Color. Keep vertex colors.
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

This first version supports face deletion, removing texture assignments, and
reassigning existing textures of the same dimensions. It validates triangle
identities and rejects added/duplicated faces, flipped winding, moved vertices,
UV changes, or changed vertex colors. Do not merge vertices, apply a decimation
modifier, or remove the `_GUD_VERTEX` attribute.

Exports separate native draw parts/material groups into objects. Distant
character LOD objects are labeled **(distant LOD)**. Edit those too when a change
should also appear at a distance; their topology can differ from the closest
LOD. The GEditor character preview continues to show the closest LOD. The
import footer counts all exported LODs, while the viewport counts displayed
geometry.

The compiler retains the native skeleton, per-vertex joint associations,
attachment points, switches, collision/bounding data and dynamic effects such
as muzzle flashes. It rebuilds triangle/material commands and omits vertex
loads unused by surviving faces. Removing visible geometry does not reshape
collision bounds or remove attachment points.

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
