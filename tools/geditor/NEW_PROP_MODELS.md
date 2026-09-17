# Adding a Blender prop to a GEditor project

New static props can be added in the Model Editor without replacing a stock
model or adding a source-code entry for each asset. A project can hold 128 added
models. `PpendantZ` can combine existing images, imported images, opaque vertex
colors and translucent vertex colors in one model.

## Prepare the project

New props require a GUD ROM with `NPRP` and `NPMD` runtime support. If the
project already supports added props, this material-slot update needs only a
rebuilt **GEditor**. Otherwise rebuild GUD with new-prop support and rebase the
project to that ROM. Existing projects without new props continue to open normally.

Import the rusty green metal image with **File > Import... > Import Image**.
This uses the existing BMP importer, including its format, palette, mipmap and
TMEM checks. The image appears in the image browser and can be assigned before
creating a new ROM. Keep image `00D4` at its existing ID. See [Image import](IMAGE_IMPORT.md) for texture preparation.

## Prepare PpendantZ

Export a static glTF 2.0 `.glb` or `.gltf` containing the pendant's meshes,
materials, UVs and vertex colors. The selected glTF scene is imported; export
only the pendant, with its attachment point at the scene origin. Multiple mesh
objects are combined, including their node transforms. Mirrored transforms
have their triangle winding corrected.

The import convention is **metres, glTF Y up**. One metre becomes 1000 native
model units, placed at the model scale of 0.1 (100 game units). Node transforms
are baked; positioning the pendant far from the Blender scene origin also
moves its imported vertices. Apply modifiers before export. The importer
accepts triangle meshes, including triangulated quads exported by Blender.

| Material slot | Blender input and GEditor assignment | Result in GUD |
| --- | --- | --- |
| Cable | UV map 0; drag image `00D4` onto its slot after import | Existing ROM image modulated by vertex RGB |
| Metal | UV map 0; drag the imported rusty green image onto its slot | New project image modulated by vertex RGB |
| Opaque untextured | No Base Color texture; `alphaMode: OPAQUE` (or omitted); `COLOR_0` RGB/RGBA | Opaque vertex-colored surface with ordinary prop room shading |
| Light shaft | No Base Color texture; `alphaMode: BLEND`; `COLOR_0` RGBA | Vertex RGB with interpolated vertex alpha, depth testing and no depth writes |

Image names are ignored during model import. Each material used by the model
gets its own thumbnail slot, initially **No Texture**, even if it has an embedded
Blender image. Give materials descriptive names such as `Cable`, `Metal`,
`Solid` and `Shaft`. Embedded image bytes do not create project images: import
the BMP into GEditor, then drag its browser thumbnail onto the desired slot.
Distinct slots remain separate even when they use the same image or No Texture.

Vertex colors hold the baked lighting. Principled roughness/metallic values,
normal maps, lights and procedural shaders are not converted to N64 lighting.
For untextured materials, use vertex colors and/or a Base Color factor. Missing
vertex colors default to white. Base Color RGBA multiplies vertex RGBA.

For the shaft, export a four-component `COLOR_0` with alpha such as
`25 / 255 = 0.0980392`. Keep the material alpha factor at 1 if that vertex value
should determine opacity by itself. If Blender exports only RGB, the missing
alpha becomes 1. The exported material must use `BLEND`; merely changing a
viewport color's alpha does not establish that glTF material mode.

Export `doubleSided: true` if the shaft should be visible from both sides.
Otherwise backface culling is used. The shaft's pass bypasses room tint and fog
so its color and opacity are authored directly. It uses explicit source-alpha
blending with either AA preference. The placed editor preview follows the same
room-tint rule; final N64 appearance still needs an in-game check.

Textures use UV map 0. Repeat, mirrored repeat and clamp are supported. Apply
mapping-node transforms to the actual UVs; `KHR_texture_transform` is rejected.
Skins, animation, morph targets, alpha-mask cutouts and generated reflection
coordinates are outside this static-prop importer. Native positions are signed
16-bit millimetres (about +/-32 metres from the origin), and UVs must fit the
native signed 16-bit 1/32-texel range. The per-model limit is 10,000 triangles;
these format limits are not performance recommendations.

## Import, place and update

1. Open the **Model Editor** and choose **Add Prop Model...**.
2. Select the exported GLB/glTF, then enter `PpendantZ`.
3. In the Model Editor's **Materials** panel, drag image `00D4` from the main
   image browser onto the cable slot. Drag the imported metal image onto the
   metal slot. Leave the solid and light-shaft slots as **No Texture**. Click a
   slot to select its faces. Drag the browser's **No Texture** item onto a slot
   to clear it. UVs are retained, including while a slot is unassigned.
4. The model also appears under **Models > Props**. Drag it into the level, then position and configure the new prop normally.
   Keep the default absolute-position flag if the pendant should retain its
   authored attachment origin.
5. **Save Project**, then create the ROM.

To change its mesh, select `PpendantZ` in the Model Editor and choose
**Import Model...**. Reimport can change topology and materials while retaining
its prop ID and existing placements. Assignments survive reimport when a slot
name matches uniquely; new, renamed or ambiguous slots start with No Texture.
Changing a slot image preserves normalized UV placement and supports different
image dimensions, subject to native UV limits. **Export Model...** exports an editable
metre-scale glTF again. The stock-model replacement workflow keeps its existing
source-identity requirements; newly added props do not need those identities.

The same Materials panel works for stock models. Their slots are derived from
the existing native parts and draw states, and initially show their current
images. Reassigning a slot affects that model and its placed instances, without
changing the image itself or other models using it.

The opaque geometry defines the collision/placement box. All geometry,
including the shaft, contributes to the culling radius. This prevents the
shaft from making the lamp's collision box extend to the floor. This version
provides a generated box, not a custom collision mesh or animated skeleton.

## Project and ROM data

Keep `models/newprops.gnp` with the project. It is the authoritative native
model bank (`GNP1`); `models/objects/PpendantZ.gltf` is a generated preview.
Edit via import, rather than modifying that preview directly. An unsaved import
is held in memory and discarded when project changes are discarded. A failed
import leaves the previous model intact. Saves replace the bank atomically.
Material names, face-slot membership and normalized UVs are stored in an
editor-only `GMS1` trailer inside each native project model. ROM export strips
that trailer: the game receives only native geometry and assigned image IDs.
A project recreated from the ROM derives slots from the native draw states;
keep the original project to retain authored slot names and unassigned UVs.

New IDs are stable in the range 512–639. This version does not delete or
renumber models. Rebase rejects a target ROM without the runtime support or
with conflicting custom model ID/name assignments. Projects made from an
exported ROM recover its custom models and save their editable bank on the
next project save.

ROM export writes the bank, updates the runtime descriptor, protects the bank
from other resource packing, and updates the checksum. Repeated export of the
same saved data reuses the bank allocation. The game loads registry metadata
per stage and loads a model's mesh only when that model is needed.

## Verification

```sh
python3 tools/geditor/tests/new_props/run.py
python3 tools/geditor/tests/model_vertex_uv/run.py
python3 tools/geditor/tests/project_rebase/run.py
python3 tools/geditor/tests/object_duplicate/run.py
python3 tools/geditor/tests/object_shading/run.py
python3 tools/geditor/tests/model_properties_ui/run.py
```

The tests cover independent unassigned slots, ignored image names, assignments
from pending project images, shared images, different image dimensions, clear/
reassign UV retention, stable size across 100 assignment cycles, stripped ROM
metadata, generated four-material GLB/glTF input, exact native RGBA and
texture IDs, display-list matrix loads, opaque/translucent passes, the native AA-off rewrite, bounds,
placed-model cache/shading, mirrored transforms, topology changes, stable IDs,
failed imports, save/reload, atomic save failure, Blender-format round trips,
ROM-bank reuse and real ROM/image export plus rebase. They do not simulate the
N64 GPU or the Windows desktop. Test the actual pendant in-game after import,
including AA on/off, placement, occlusion and overlapping shaft faces.
