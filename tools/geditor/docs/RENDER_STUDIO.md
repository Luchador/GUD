# Render Studio

Open a project, then choose **Tools > Render Studio**. This opens a separate,
resizable window while the main editor remains usable. Choosing the menu item
again brings the existing window forward.

The workspace contains:

- Scene, Images, and Models panels on the left.
- An independent OpenGL viewport in the center.
- Scene Outliner, Materials, and Properties panels on the right, in that order.

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
Each drop creates a separate instance with independent material settings, while
instances of the same file share loaded geometry. The scene outliner identifies
each instance by model filename and instance number.

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

The interactive preview uses a directional light, ambient fill, and per-vertex
Phong lighting. Base images affect diffuse color without tinting the separate
specular highlight. This is an opaque preview; glTF PBR roughness/metallic maps,
transparency, scene light editing, and the final SGI-style renderer are future work.
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
assets, and per-instance materials. A `.rnd` is a UTF-8 JSON document with
`"format": "GEditor Render Studio"`, `"version": 1`, and an `objects` array.
Existing empty version-1 scenes remain supported. Each object stores its model
leaf filename, three-component position, and material overrides (`name`, `image`,
`base`, `specular`, `intensity`, and `shininess`). Paths are relative to the
project's studio folders. The scene name comes from its filename.
`gltf.c` provides a separate studio import mode using the shared JSON codec in
`gltfjson.c`; it does not require game source identities or game texture tags.
`studiomath.c` owns ray picking, bounds, and preview shading. `studioviewport.c`
owns the preview camera, rendering, image textures, and its own OpenGL context.

Run `python3 tools/geditor/tests/studio_materials/run.py` for glTF/material,
scene persistence, failure rollback, picking, and lighting regression coverage.
The suite uses production code with a filesystem shim and ASan/UBSan. Native
Windows interaction, WIC texture decoding, and window layout need a Windows
smoke test: drop a three-material model twice, edit one instance, change selection,
reopen the scene, and resize the window while an image is assigned.
