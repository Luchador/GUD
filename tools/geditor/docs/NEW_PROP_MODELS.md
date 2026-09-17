# PpendantZ: Blender-to-GEditor walkthrough

This walkthrough accompanies the new-prop-models patch and its missing-header
build fix. The generic pipeline reference follows below. You can put this file
at `tools/geditor/NEW_PROP_MODELS.md` in your checkout.

## What is in the supplied pendant

The supplied `PpendantZ.glb` has 80 triangles and four materials. All four
parts contain RGBA vertex colors, currently white with alpha 1 (255/255).
All materials currently export as opaque, including `untexted_alpha`.
The two exported image names are `00D4` and `PpendantZ`; GEditor requires the
`GUD Image ` prefix described below. The BMP is 48 x 32 pixels, RGB.

| Part / material | Triangles | Preparation |
| --- | ---: | --- |
| Cable: `00D4` | 10 | Name its image datablock `GUD Image 00D4`. |
| Metal: `PpendantZ` | 34 | Import the BMP into GEditor, then use `GUD Image XXXX` with that image's assigned ID. |
| Solid untextured: `untextured` | 20 | Keep its shader Alpha at 1; paint RGB if desired. |
| Shaft: `untexted_alpha` | 16 | Set up shader Alpha as described below. |

## 1. Prepare GEditor and import the texture

Rebuild GEditor as well as GUD. Open your project and rebase it onto the newly
built GUD ROM if you have not already done that. Building GUD by itself does
not update an existing project's base ROM.

Choose **File > Import... > Import Image** and open `PpendantZ.bmp`.
For a straightforward first import, use **RGBA16** and **None (base image only)**
for mipmaps. The 48 x 32 image takes 3072 of 4096 TMEM bytes in this format.
Choose the appropriate metal hit/bullet-hole settings, import, and record the
four-digit hexadecimal image ID. Save the project.

The ID depends on your project; do not assume an example ID is the right one.

## 2. Bind the two images in Blender

In the Shader Editor, rename the actual image datablock used by each
Image Texture node. Changing only the material name or the node's label is
not sufficient. The Image Editor's image datablock name field can also be used.

- Existing cable image: `GUD Image 00D4`.
- Rusty-metal image: `GUD Image XXXX`, replacing `XXXX` with the ID just assigned.

Keep each Image Texture Color output connected to its Principled BSDF Base
Color. The two untextured materials must have no Base Color image texture.
The GLB's embedded images are references/previews; GEditor does not import them
as new ROM images automatically.

## 3. Choose the shaft opacity workflow

### Uniform opacity: simplest first test

Select `untexted_alpha`. Set its **Principled BSDF > Alpha** to `25/255`
(approximately `0.0980392`). Leave vertex alpha at 1. GEditor multiplies the
material alpha by vertex alpha and stores the result in native vertex alpha,
so this produces 25/255 across the shaft.

Set the shaft's Base Color to the intended light color. This must be the shader
input, not the separate Material Viewport Display color. For Blender preview,
use a transparency-capable surface render method such as Dithered and Material
Preview. The Alpha shader input is what the recent glTF exporter inspects;
selecting Dithered alone does not establish translucency in the exported GLB.

### Painted fade

Blender's Vertex Paint RGB picker does not expose alpha as a fourth paint color.
The Draw/Paint brush has **Blend > Erase Alpha** and **Blend > Add Alpha**:
erasing lowers opacity; adding raises it. Strength controls the brush effect,
not an exact final alpha value.

1. In **Object Data Properties > Color Attributes**, select the intended color
   attribute. For a new attribute, Face Corner / Byte Color is suitable; start
   at white with alpha 1. A Face Corner attribute can give adjacent opaque and
   transparent faces different colors/alpha even where they share a vertex.
2. In the shaft material's Shader Editor, add a **Color Attribute** node and
   select that same attribute.
3. Connect its **Alpha** output to a **Math** node set to **Multiply**. Set the
   other input to `25/255`. Connect the result to **Principled BSDF > Alpha**.
4. Keep the shaft's Base Color at the desired constant color, or connect the
   Color Attribute node's **Color** output to Base Color if you also want to
   paint its RGB. A direct Color connection replaces the former constant tint.
5. Select only the shaft faces and enable face selection masking for painting.
   In **Vertex Paint**, use the Draw/Paint brush and change **Blend** from Mix
   to **Erase Alpha**. Paint the end of the shaft towards transparency. Use
   **Add Alpha** to restore opacity. Switch back to **Mix** for RGB painting.
6. Check the result in Material Preview using the material above. The plain
   vertex-color display does not by itself demonstrate shader transparency.

With this setup, vertex alpha is a 0-to-1 fade mask and the multiplier sets
maximum opacity to 25/255. Do not also set every vertex alpha to 25/255, or the
two values will multiply to about 1% opacity. For directly authored final
vertex alpha, omit the Math multiplier and connect the attribute Alpha straight
to the shader Alpha instead.

Keep the other three materials' Alpha inputs at 1. Use backface culling off
for the shaft if it should be visible from both sides; your supplied GLB
already marks all four materials double-sided.

## 4. Export the GLB

Select the pendant and use **File > Export > glTF 2.0**:

- Format: **glTF Binary (.glb)**.
- Include: **Selected Objects**; export the mesh and its materials.
- Keep UVs and vertex colors. Under vertex-color export options, choose
  **Use Vertex Color: Active**, with the intended Color Attribute active.
  Exporting all attributes alone can produce a dummy white COLOR_0 when the
  material does not use vertex colors; GEditor reads COLOR_0.
- Disable mesh/Draco compression and animation export for this static prop.
- Use the first UV map. Bake Mapping-node changes into UV coordinates.

Set the model's ceiling attachment point at the Blender scene origin before
export if you want placement to use that point. Moving the object origin alone
without moving the object to world zero does not change exported world-space
geometry. To do this deliberately: place the 3D cursor at the attachment point,
use Object > Set Origin > Origin to 3D Cursor, then clear object location.

Check scale before export: the uploaded GLB's shade is approximately 2 metres
wide, and its total height including cable and shaft is approximately 5.76
metres. GEditor interprets glTF dimensions as metres. Keep those dimensions if
intentional, or resize before export. Its current attachment is about 3 metres
above scene zero.

The shaft material should now export as `alphaMode: BLEND`; low vertex alpha
alone on an opaque material is insufficient. Its COLOR_0 should contain the
intended RGBA values. The exporter recognizes a Color Attribute Alpha output
and an optional constant Math Multiply in the shader's alpha connection.

## 5. Add and place the prop

1. In GEditor choose **Tools > Model Editor > Add Prop Model...**.
2. Pick the new GLB and name the prop `PpendantZ`.
3. Inspect all four materials, then find it under **Models > Props** and drag it
   into the level. Keep the default absolute-position flag for an authored
   attachment origin.
4. **Save Project**, then **Tools > Create ROM**. Check it in-game with AA on
   and off, and from both sides of the shaft.

For subsequent Blender changes, select this existing model and use
**Import Model...**. This retains its model ID and existing placements.

If import reports an unidentified texture, check the image datablock names.
If it reports a missing project image, import that image first and use the
assigned ID. If the shaft stays opaque, check the shader Alpha connection and
re-export rather than only changing Blender's viewport settings.

## Blender source references

- [Vertex-paint alpha blending](https://github.com/blender/blender/blob/v4.5.0/source/blender/editors/sculpt_paint/paint_vertex.cc)
- [Brush blend names](https://github.com/blender/blender/blob/main/source/blender/makesrna/intern/rna_brush.cc)
- [glTF shader-alpha detection](https://github.com/KhronosGroup/glTF-Blender-IO/blob/main/addons/io_scene_gltf2/blender/exp/material/search_node_tree.py)
- [glTF vertex-color export options](https://github.com/KhronosGroup/glTF-Blender-IO/blob/main/addons/io_scene_gltf2/__init__.py)

---

# Adding a Blender prop to a GEditor project

New static props can be added in the Model Editor without replacing a stock
model or adding a source-code entry for each asset. A project can hold 128 added
models. `PpendantZ` can combine existing images, imported images, opaque vertex
colors and translucent vertex colors in one model.

## Prepare the project

Apply this change, rebuild **both GUD and GEditor**, then use GEditor's project
rebase command to make a project based on the rebuilt GUD ROM. Its manifest must
include `NPRP` and `NPMD`; an older ROM cannot load these additional model IDs.
Existing projects without new props continue to open normally.

Import the rusty green metal image with **File > Import... > Import Image**.
This uses the existing BMP importer, including its format, palette, mipmap and
TMEM checks. Note the hexadecimal ID it assigns. Keep image `00D4` at its
existing ID. See [Image import](IMAGE_IMPORT.md) for texture preparation.

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

| Material | Exported glTF input | Result in GUD |
| --- | --- | --- |
| Existing image 00D4 | Base Color texture; image name `GUD Image 00D4`; UV map 0 | Existing ROM image modulated by vertex RGB |
| Rusty green metal | Base Color texture; image name `GUD Image XXXX`, using the newly imported ID | New project image modulated by vertex RGB |
| Opaque untextured | No Base Color texture; `alphaMode: OPAQUE` (or omitted); `COLOR_0` RGB/RGBA | Opaque vertex-colored surface with ordinary prop room shading |
| Light shaft | No Base Color texture; `alphaMode: BLEND`; `COLOR_0` RGBA | Vertex RGB with interpolated vertex alpha, depth testing and no depth writes |

Use the `GUD Image XXXX` name on the image data used by each textured material;
material slot names are unrestricted. The exported `images[].name` is what
identifies the image, so check it if the importer reports an unidentified
texture. Image bytes embedded in the GLB do **not** create or replace project
images. Import images in GEditor first.

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
3. Inspect it in the Model Editor. It also appears under **Models > Props**.
4. Drag it into the level, then position and configure the new prop normally.
   Keep the default absolute-position flag if the pendant should retain its
   authored attachment origin.
5. **Save Project**, then create the ROM.

To change its mesh, select `PpendantZ` in the Model Editor and choose
**Import Model...**. Reimport can change topology and materials while retaining
its prop ID and existing placements. **Export Model...** exports an editable
metre-scale glTF again. The stock-model replacement workflow keeps its existing
source-identity requirements; newly added props do not need those identities.

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

The tests cover generated four-material GLB/glTF input, exact native RGBA and
texture IDs, display-list matrix loads, opaque/translucent passes, the native AA-off rewrite, bounds,
placed-model cache/shading, mirrored transforms, topology changes, stable IDs,
failed imports, save/reload, atomic save failure, Blender-format round trips,
ROM-bank reuse and real ROM/image export plus rebase. They do not simulate the
N64 GPU or the Windows desktop. Test the actual pendant in-game after import,
including AA on/off, placement, occlusion and overlapping shaft faces.
