# Model vertex painting

Open a model and click the **Vertex Paint** icon beside the import/export
buttons, or press **4** (also numpad 4). Press it again to return to face
selection. Shortcuts leave model names and RGBA text fields alone.

The picker between **Materials** and **Face properties** provides HSV controls
and R, G, B and A fields (0–255). Click a visible face near a vertex to paint
its RGBA. Corners sharing the same native vertex update together; separate
vertices at UV/color seams remain separate. Drag the left or right mouse button
to orbit, hold the middle button and drag to pan, and scroll to zoom. Panning
moves the orbit target with the camera, so you can focus on a small model part.
Camera drags do not paint. **Pick Color** samples the nearest vertex's authored
RGBA; Escape cancels sampling.

Use **Ctrl+Z** to undo painting and **Ctrl+Y** or **Ctrl+Shift+Z** to redo it.
The current model keeps up to 256 paint steps. Switching models or changing
materials, face properties or imported geometry clears that paint history.
Save Project persists the colors for subsequent model exports and ROM builds.
Painting edits the model asset, so all placed instances use the updated colors.

Alpha painting preserves the face's render mode. For translucent surfaces,
select their faces and use **Render mode → Alpha blend** before painting.
Stock model parts that store lighting normals instead of RGB, or use material
constants for opacity, retain the existing Blender-import restrictions. An
unsupported paint reports the reason in the status line and changes nothing.

Regression checks: `tests/model_paint_ui`, `tests/model_properties_ui`,
`tests/model_vertex_uv`, `tests/new_props`, `tests/vertex_eyedropper` and
`tests/geometry_toolbar` each provide a `run.py` driver. These cover layout and
input using window stubs and native save/export paths using production code;
interactive Win32/OpenGL appearance still requires a Windows smoke check.
