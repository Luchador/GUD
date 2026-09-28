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
Press **Z** with the viewport focused to frame the selected model or light; with
nothing selected, it frames the whole scene, including its lights. The grid and colored X/Y/Z axes
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
- **Emission:** a color swatch, initially black. Adds self-illumination even with
  every light off. It is independent of base color, base image, vertex colors,
  metalness, and the specular controls. Emission does not light other objects
  or add a bloom/glow effect around the model.
- **Phong specular color:** a color swatch, initially white. Controls the
  nonmetallic portion of the highlight.
- **Phong specular intensity:** 0–1, initially 0.25.
- **Shininess:** 1–128, initially 32; higher values narrow the highlight.
- **Metalness:** a 0–100% slider, initially 0%. Increasing it reduces diffuse
  lighting and blends from the chosen specular color toward the base color,
  vertex colors, and base image for the highlight. At 100%, the material has
  only metallic highlights plus any emission. Specular intensity still controls
  highlight strength, and shininess controls its width. Intermediate values
  mix the nonmetallic and metallic responses. The slider displays its percentage,
  previews while dragging, and saves when released; arrow keys adjust by 1% and
  Page Up/Down by 10% while it has focus.

Metalness is an artistic extension of the Phong preview, not a PBR renderer.
Place lights where they produce visible highlights, and adjust specular intensity
and shininess for the finish you want. There are no environment or scene
reflections yet, so fully metallic surfaces can be dark away from highlights.

Color and image choices apply immediately. Numeric values apply when pressing
Enter or leaving the field. Completed edits and placements automatically save
the `.rnd` scene. Invalid edits and failed saves restore the previous setting;
an existing scene file is replaced only after its complete replacement is written.

Every scene has permanent **Ambient Light** and **Directional Light** entries
in the Scene Outliner. They cannot be added or deleted. Select either entry to
edit **Color** and **Intensity** (0–10000) in **Properties**; intensity 0 turns
that light off. New scenes start with white ambient light at 0.2 and white
directional light at 1. Ambient light fills all surfaces equally and has no
specular highlight. Directional light has no position or distance falloff.

Select **Directional Light** to edit **Direction X/Y/Z** in **Transform**.
This nonzero vector points in the direction the light travels, as with a
spotlight, and is normalized for rendering. The permanent lights have no
viewport icons or transform gizmos; their position and scale fields are disabled.
All light contributions are independent: adding or removing a spotlight or point
light does not change the ambient or directional light's settings.

Choose **Add > Spotlight** or **Add > Point Light** next to the File menu. Each
scene supports **one spotlight and two point lights**; an item is disabled when
its limit is reached. If no scene is open, the New Scene prompt appears first.
New lights start above the models (or at 0, 5, 0 in an empty scene), white, with
intensity 1. The spotlight points down, with inner/outer angles of 20/30 degrees.
Point lights start with a radius sized to the models, or 10 in an empty scene.

Select **Spotlight**, **Point Light 1**, or **Point Light 2** in the Scene Outliner,
or click its icon in the viewport. The supplied spotlight and point-light icons
are embedded in GEditor, centered on each light's position, and remain 32 pixels
wide while zooming. They stay visible over geometry; the closest icon wins when
icons overlap. The selected icon is gold. A selected spotlight also shows an
arrow indicating its direction.

The **Transform** panel shows **Position X/Y/Z** for both light types and
**Direction X/Y/Z** for spotlights. Direction is a nonzero vector, not a target
position or Euler angles; it is normalized for rendering. Light positions use
the same units as model positions. Use **W** and the move gizmo for either type.
Use **E** and the rotation gizmo to aim the spotlight, with **Ctrl** for 10-degree
snapping. The direction fields update during the drag. **Escape**, loss of focus
or capture, resizing, or switching selection/scene cancels an unfinished drag.
Point lights have no orientation, and lights have no model scale, so their
unsupported rotation/scale controls are disabled. Models retain W/E/R and the
usual Position, Rotation, and Scale fields.

**Properties** contains the remaining light settings:

- Both types have **Color** and **Intensity** (0–10000). Intensity 0 turns off
  that light's contribution.
- A spotlight has an **Inner angle** and an **Outer angle**, in degrees from the
  direction axis (half the full cone width).
  The inner angle must be between 0 and the outer angle; the outer angle must be
  greater than 0 and no more than 90. Brightness is full inside the inner cone,
  fades smoothly between the cones, and is zero outside. Equal angles make a
  hard cone edge. Spotlight brightness has no distance falloff.
- A point light has a **Radius** in scene units, from 0.0001 to 1 billion.
  Brightness fades as `(1 - distance / radius)^2`, reaching zero at the radius.

Color changes apply immediately; press Enter or leave a numeric field to apply
it. Gizmo edits preview continuously and save on release. Light creation and
edits save automatically, and File > Save Scene/Ctrl+S includes them. Invalid
edits or failed saves restore the previous settings.

Press **Delete** with the viewport or Scene Outliner focused to remove the
selected spotlight or point light and save the scene. Delete still edits text normally in numeric
fields. A failed save restores the light and its selection. Deleting a light
frees its Add-menu slot; the other lights retain their identities and settings.

The interactive preview combines the scene's ambient fill with per-vertex
Phong lighting from its directional light, spotlight, and point lights.
The preview interpolates lighting across triangles, so cone edges and small
point-light footprints are more accurate on meshes with sufficient vertices.
Base images tint diffuse color and the metallic portion of highlights. The
nonmetallic highlights and emission remain independent of that image.
The renderer draws diffuse first, then adds nonmetallic highlights/emission,
then textured metallic highlights only for materials with nonzero metalness.
This is an opaque preview; glTF PBR roughness/metallic maps,
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
assets, per-instance materials, fixed local-light slots, and permanent lighting. A `.rnd` is a UTF-8 JSON
document with
`"format": "GEditor Render Studio"`, `"version": 5`, `objects` and `lights` arrays,
and required `ambient` and `directional` objects. Each permanent-light object
stores `color` (RGB, 0–1) and `intensity`; `directional` also stores `direction`.
Versions 1–4 remain supported. Versions 1–3 use the legacy lighting defaults:
their ambient light defaults to white at 0.2.
Their directional light preserves the original preview direction and is white
at intensity 1 if there are no local lights, or intensity 0 if any local light
exists (including one with zero intensity). This preserves their previous
appearance. After loading, the permanent lights can be edited independently.
For version-1 scenes, existing positions and material settings are
retained, rotation defaults to zero, and scale defaults to one. An empty scene
created by New Scene starts as version 1; saving upgrades it to version 5.
Each object stores its model leaf filename, three-component `position`, `rotation`
(in degrees), `scale` (multipliers), and material overrides (`name`, `image`,
`base`, `specular`, `intensity`, `shininess`, `emission`, and `metalness`).
Version 5 requires `emission` (three RGB values, 0–1) and `metalness` (0–1) on each
saved material. Versions 1–4 default both to zero, preserving their appearance;
version 4 retains its saved global-light settings. New imported material slots
also start with black emission and zero metalness, irrespective of glTF PBR
material settings. Paths are relative to the
project's studio folders. The scene name comes from its filename.
Each light stores `type` (`spotlight` or `point`), `slot`, `position`, `color`,
and `intensity`. The optional `slot` preserves point-light identity after deletions;
older version-3 scenes without it assign slots in file order. Spotlights also store `direction`, `inner`, and `outer`; point lights
store `radius`. Invalid light data or exceeded limits reject the entire load
without changing the open scene. Older editor builds reject version-5 scenes
rather than silently discarding the new material settings.
`gltf.c` provides a separate studio import mode using the shared JSON codec in
`gltfjson.c`; it does not require game source identities or game texture tags.
`studiomath.c` owns model matrices, ray picking, transformed bounds, and preview
shading. `studiodrag.c` handles transform interaction math; `studiogizmo.c` loads,
draws, and picks the same arrow/ring/scale assets used by the main editor.
`studiolightview.c` owns the embedded light icons, projection, picking, and direction
guide. `studioviewport.c`
owns the preview camera, rendering, image textures, and its own OpenGL context.

Run `python3 tools/geditor/tests/studio_materials/run.py` for glTF/material,
scene persistence, failure rollback, picking, and lighting regression coverage,
including emission, metalness blending, material validation, and v1–v4 migration.
Run `python3 tools/geditor/tests/studio_lights/run.py` for light limits, properties,
roundtrip persistence, legacy migration, invalid-document/save rollback, cone and
radius falloff, colored diffuse/specular contributions, and outliner/property
callback coverage with stand-in native controls, light transforms, icon picking,
framing, deletion, stable slot reuse, permanent-light controls/non-deletion,
colored ambient/directional illumination, v1–v3 appearance migration, and the
emission picker/metalness slider callbacks with drag commit and save rollback.
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
between models/materials/lights, click each light icon, move it with W, rotate the
spotlight with E, cancel a drag with Escape, and edit Position/Direction in
Transform. Delete a light from both the viewport and outliner, add a replacement,
then save and reopen the scene. Confirm Delete in a numeric field only edits text.

Select Ambient Light and Directional Light, change their colors and intensities,
edit Direction in Transform, and confirm Delete cannot remove either. Set both
intensities to zero and remove local lights to check the model becomes unlit;
restore directional intensity, add a point light, then save/reopen and confirm
both contributions and settings are retained.

For the new material controls, choose a colored Emission with all lights off;
confirm the color remains visible with a black base color or a dark base image.
Try metalness at 0%, 50%, and 100% under a directional or local light, with and
without a base image, then adjust intensity and shininess. Drag the slider,
change material/scene, and save/reopen to confirm settings stay with the selected
material instance. Confirm the controls disappear when selecting a light.
