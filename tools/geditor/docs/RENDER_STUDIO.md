# Render Studio

Open a project, then choose **Tools > Render Studio**. This opens a separate,
resizable window while the main editor remains usable. Choosing the menu item
again brings the existing window forward.

The workspace contains:

- Scene, Images, and Models panels on the left.
- An independent OpenGL viewport in the center.
- Scene Outliner, Transform, Materials, and Properties panels on the right, in that order.

Drag with either the left or right mouse button to orbit, drag with the middle
button to pan, and use the wheel to zoom. Click a model to select its instance
and the material under the cursor, or select its entry in the Scene Outliner.
Press **Z** with the viewport focused to frame the selected instance; with no
instance selected, it frames the whole scene. The grid and colored X/Y/Z axes
are preview guides.

Choose **File > New Scene...** in Render Studio, enter a name, then click **OK**
to create an empty `.rnd` scene in `[Project Name]/studio/scenes`. **Cancel** (or
Escape) closes the prompt without creating a file. The `.rnd` extension is added
automatically; including it in the name is also accepted. Invalid filenames and
names already in use leave the prompt open with an explanation. Existing scenes
are never overwritten, and a failed save does not publish a partial scene.

The Scene panel lists the project's `.rnd` filenames in alphabetical order and
opens a newly created scene. Select a filename to open that scene. The first
scene is opened automatically when starting Render Studio or switching projects.
The list refreshes when Render Studio is opened, regains focus, or changes projects.

The Models panel lists `.gltf` files in `studio/models`. The Images panel displays
`.bmp` files in `studio/images` using the main editor's thumbnail grid, scrolling,
selection highlight, and hover tooltips. Filenames are sorted alphabetically;
extension matching is case-insensitive. Lists refresh when Render Studio opens,
regains focus, or switches projects, so files copied in Explorer appear when you
return to the window. Only files directly in these folders are listed.

Studio thumbnails preserve ordinary BMP orientation and aspect ratio, including
images larger than the game's texture limits. Hover over a thumbnail to see its
full filename and dimensions. An unreadable BMP remains listed with **No preview**.

Drag a model from **Models** into the viewport to create an instance. Its base
rests on the ground plane at the drop point, and the first placement is framed
automatically. If there is no open scene, the New Scene prompt appears first.
Each drop creates a separate instance with independent transforms and material settings, while
instances of the same file share loaded geometry. The scene outliner identifies
each instance by model filename and instance number.

Choose **File > Save Scene** or press **Ctrl+S** in Render Studio to save the
current scene's model instances, transforms, material properties, and lights. Pending
numeric edits are applied before saving. Completed placements and edits also
continue to save automatically.

Select a model in the viewport or Scene Outliner to show its transform gizmo and
**Transform** panel. Use the panel buttons or **W** for translation, **E** for
rotation, and **R** for scaling. These shortcuts belong to Render Studio and do
not activate while typing in a text field or using a drop-down.

- Drag the red, green, or blue handle to transform along/about X, Y, or Z.
- Translation and rotation use world axes. Scaling follows the model's rotated
  local axes. The white center cube scales all three axes proportionally.
- Hold **Ctrl** while rotating to snap to 10-degree increments.
- **Escape** cancels an active transform and restores its starting values.
  Losing capture/focus or switching scenes also cancels an unfinished drag.
- The Transform panel provides Position, Rotation, and Scale X/Y/Z values.
  Rotation is in degrees (Euler order Z * Y * X); scale is a multiplier, with 1
  meaning the original size. Scale stays positive, between 0.0001 and 10000.
  Press Enter or leave a numeric field to apply it.

Transforms use the model's imported origin as their pivot. Gizmos stay a usable
size as the camera zooms and remain visible over the model. Dragging previews the
transform continuously; releasing the mouse saves it. Bounds, face picking,
texture coordinates, and lighting follow the transformed instance, including
correct normals under nonuniform scale. Failed saves restore the previous transform.

Static glTF meshes retain their material names, UVs, vertex colors (when present),
base colors, and transformed normals. Missing normals are generated per face.
The default glTF scene and its node transforms are used, with embedded or external
buffers. Keep external `.bin` dependencies beside the `.gltf` file. Apply skinning,
morph targets, and animations before exporting a static studio model.

The **Materials** panel lists the selected instance's material slots. Selecting
a slot exposes these controls in **Properties**:

- **Base color:** a color swatch that opens the color picker. Defaults to the
  glTF material's base color, or white when absent.
- **Base image:** None or a `.bmp` from `studio/images`, sampled with the model's
  UVs and multiplied by its base color. Starts at None; glTF texture references
  are not automatically converted into studio image assignments.
- **Phong specular color:** a color swatch, initially white.
- **Phong specular intensity:** 0–1, initially 0.25.
- **Shininess:** 1–128, initially 32; higher values narrow the highlight.

Color and image choices apply immediately. Numeric values apply when pressing
Enter or leaving the field. Completed edits and placements automatically save
the `.rnd` scene. Invalid edits and failed saves restore the previous setting;
an existing scene file is replaced only after its complete replacement is written.

Choose **Add > Spotlight** or **Add > Point Light** next to the File menu. Each
scene supports **one spotlight and two point lights**; an item is disabled when
its limit is reached. If no scene is open, the New Scene prompt appears first.
New lights start above the models (or at 0, 5, 0 in an empty scene), white, with
intensity 1. The spotlight points down, with inner/outer angles of 20/30 degrees.
Point lights start with a radius sized to the models, or 10 in an empty scene.

Select **Spotlight**, **Point Light 1**, or **Point Light 2** in the Scene Outliner
to edit the light in **Properties**:

- Both types have **Position X/Y/Z**, **Color**, and **Intensity** (0–10000).
  Intensity 0 turns off that light's contribution.
- A spotlight also has a nonzero **Direction X/Y/Z**, an **Inner angle**, and an
  **Outer angle**, in degrees from the direction axis (half the full cone width).
  Direction is normalized for rendering; it is a vector, not a target position.
  The inner angle must be between 0 and the outer angle; the outer angle must be
  greater than 0 and no more than 90. Brightness is full inside the inner cone,
  fades smoothly between the cones, and is zero outside. Equal angles make a
  hard cone edge. Spotlight brightness has no distance falloff.
- A point light has a **Radius** in scene units, from 0.0001 to 1 billion.
  Brightness fades as `(1 - distance / radius)^2`, reaching zero at the radius.

Light positions use the same units as model positions. Edit them in Properties;
the Transform panel and viewport gizmos currently operate on models. Color changes
apply immediately; press Enter or leave a numeric field to apply it. Light
creation and edits save automatically, and File > Save Scene/Ctrl+S includes them.
Invalid edits or failed saves restore the previous settings.

The interactive preview uses ambient fill and per-vertex Phong lighting. Once a
light is added, the scene's lights supply the diffuse and specular illumination.
Scenes without added lights retain the original default directional light.
The preview interpolates lighting across triangles, so cone edges and small
point-light footprints are more accurate on meshes with sufficient vertices.
Base images affect diffuse color without tinting the separate
specular highlight. This is an opaque preview; glTF PBR roughness/metallic maps,
transparency, shadows, and the final SGI-style renderer are future work.
BMP preview textures use power-of-two dimensions up to 4096 for OpenGL compatibility.

Scene loading preserves unavailable model instances and their material settings,
and marks them **unavailable** in the outliner. Restore the model and reopen the
scene to resolve it. Materials are matched by slot and name, with a unique-name
fallback when slots are reordered in Blender. Reopen a scene after changing a
model externally; image files refresh when Render Studio regains focus.

Each project has `studio/models`, `studio/images`, and `studio/scenes` folders.
New projects create them automatically. Opening an older project adds missing
folders without changing existing files. Opening Render Studio also checks
these folders. An existing `studio/model` folder is renamed to `studio/models`,
moving its complete contents, including glTF dependencies. If both folders already
exist, both are preserved and the Models panel uses `studio/models`; consolidate
the legacy folder manually if needed. The existing `.gep` format is unchanged.

Studio assets are separate from the game's `models` and `images` folders and
are not compiled into a ROM. Rebase Project preserves the entire `studio`
folder with the other project files. Switching projects resets the studio
workspace to the newly opened project; switching game levels does not.

`renderstudio.c` owns the window, layout, and project context.
`studioassets.c` enumerates studio files and builds the image thumbnail catalog.
`browser.c` provides the same image grid for the main editor and Render Studio.
`studioscene.c` owns scene filenames, creation, and enumeration.
`studiodocument.c` owns scene loading, atomic replacement, instances, shared model
assets, per-instance materials, and fixed light slots. A `.rnd` is a UTF-8 JSON
document with
`"format": "GEditor Render Studio"`, `"version": 3`, an `objects` array, and a
`lights` array. Version-1 and version-2 scenes remain supported without lights.
For version-1 scenes, existing positions and material settings are
retained, rotation defaults to zero, and scale defaults to one. An empty scene
created by New Scene starts as version 1; saving upgrades it to version 3.
Each object stores its model leaf filename, three-component `position`, `rotation`
(in degrees), `scale` (multipliers), and material overrides (`name`, `image`,
`base`, `specular`, `intensity`, and `shininess`). Paths are relative to the
project's studio folders. The scene name comes from its filename.
Each light stores `type` (`spotlight` or `point`), `position`, `color`, and
`intensity`. Spotlights also store `direction`, `inner`, and `outer`; point lights
store `radius`. Invalid light data or exceeded limits reject the entire load
without changing the open scene. Older editor builds reject version-3 scenes
rather than silently discarding their lights.
`gltf.c` provides a separate studio import mode using the shared JSON codec in
`gltfjson.c`; it does not require game source identities or game texture tags.
`studiomath.c` owns model matrices, ray picking, transformed bounds, and preview
shading. `studiodrag.c` handles transform interaction math; `studiogizmo.c` loads,
draws, and picks the same arrow/ring/scale assets used by the main editor. `studioviewport.c`
owns the preview camera, rendering, image textures, and its own OpenGL context.

Run `python3 tools/geditor/tests/studio_materials/run.py` for glTF/material,
scene persistence, failure rollback, picking, and lighting regression coverage.
Run `python3 tools/geditor/tests/studio_lights/run.py` for light limits, properties,
roundtrip persistence, legacy migration, invalid-document/save rollback, cone and
radius falloff, colored diffuse/specular contributions, and outliner/property
callback coverage with stand-in native controls.
Run `python3 tools/geditor/tests/studio_transforms/run.py` for transform
persistence, legacy-scene loading, transformed picking/lighting, drag math,
actual gizmo assets, hotkey routing, and Save Scene input ordering.
Run `python3 tools/geditor/tests/studio_drag/run.py` for the Models-list mouse
callback, including native list-box capture notifications and drag cancellation.
The suite uses production code with a filesystem shim and ASan/UBSan. Native
Windows interaction, WIC texture decoding, and window layout need a Windows
smoke test: drop a three-material model twice, edit one instance, change selection,
move/rotate/scale it using both handles and numeric fields, save and reopen the
scene, and resize the window while an image is assigned. Add one spotlight and
two point lights, confirm the Add menu limits, edit every light property, switch
between models/materials/lights, then save and reopen the scene.
