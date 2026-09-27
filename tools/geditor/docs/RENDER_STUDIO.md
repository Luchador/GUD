# Render Studio

Open a project, then choose **Tools > Render Studio**. This opens a separate,
resizable window while the main editor remains usable. Choosing the menu item
again brings the existing window forward.

The workspace contains:

- Scene, Images, and Models panels on the left.
- An independent OpenGL viewport in the center.
- Scene Outliner and Properties panels on the right.

Drag with either the left or right mouse button to orbit, drag with the middle
button to pan, and use the wheel to zoom. The grid and colored X/Y/Z axes are
preview guides. Asset import, scene loading/editing, lighting, and the SGI-style
renderer will be added separately.

Choose **File > New Scene...** in Render Studio, enter a name, then click **OK**
to create an empty `.rnd` scene in `[Project Name]/studio/scenes`. **Cancel** (or
Escape) closes the prompt without creating a file. The `.rnd` extension is added
automatically; including it in the name is also accepted. Invalid filenames and
names already in use leave the prompt open with an explanation. Existing scenes
are never overwritten, and a failed save does not publish a partial scene.

The Scene panel lists the project's `.rnd` filenames in alphabetical order and
selects a newly created scene. The list refreshes when Render Studio is opened,
regains focus, or changes projects. Selecting a filename does not yet load it.

The Models panel lists `.gltf` files in `studio/models`. The Images panel displays
`.bmp` files in `studio/images` using the main editor's thumbnail grid, scrolling,
selection highlight, and hover tooltips. Filenames are sorted alphabetically;
extension matching is case-insensitive. Lists refresh when Render Studio opens,
regains focus, or switches projects, so files copied in Explorer appear when you
return to the window. Only files directly in these folders are listed.

Studio thumbnails preserve ordinary BMP orientation and aspect ratio, including
images larger than the game's texture limits. Hover over a thumbnail to see its
full filename and dimensions. An unreadable BMP remains listed with **No preview**.
These panels browse assets; placement and material assignment will be added later.

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
`studioscene.c` owns scene filenames, creation, and enumeration. An empty `.rnd`
is a UTF-8 JSON document with `"format": "GEditor Render Studio"`, `"version": 1`,
and `"objects": []`. Its name is supplied by the filename. The version field
allows the format to evolve as scene editing is added.
`studioviewport.c` owns the preview camera and OpenGL context. It shares only
the editor's orbit-camera math, with no dependency on the game renderer or
game asset formats.
