# Render Studio

Open a project, then choose **Tools > Render Studio**. This opens a separate,
resizable window while the main editor remains usable. Choosing the menu item
again brings the existing window forward.

The initial workspace contains:

- Scene, Images, and Models panels on the left.
- An independent OpenGL viewport in the center.
- Scene Outliner and Properties panels on the right.

Drag with either the left or right mouse button to orbit, drag with the middle
button to pan, and use the wheel to zoom. The grid and colored X/Y/Z axes are
preview guides. This initial version has an empty scene; asset import, scene
editing/saving, lighting, and the SGI-style renderer will be added separately.

Each project has `studio/model` and `studio/images` folders. New projects create
them automatically. Opening an older project adds missing folders without
changing existing files. Opening Render Studio also checks these folders.
The existing `.gep` format is unchanged.

Studio assets are separate from the game's `models` and `images` folders and
are not compiled into a ROM. Rebase Project preserves the entire `studio`
folder with the other project files. Switching projects resets the studio
workspace to the newly opened project; switching game levels does not.

`renderstudio.c` owns the window, layout, and project context.
`studioviewport.c` owns the preview camera and OpenGL context. It shares only
the editor's orbit-camera math, with no dependency on the game renderer or
game asset formats.
