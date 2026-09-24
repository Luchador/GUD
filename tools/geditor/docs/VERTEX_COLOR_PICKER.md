# Vertex color picker

Choose **Paint Vertices** (5), then click **Pick Color** above the HSV controls
in the right panel. The viewport cursor becomes a crosshair. Click a background
face near the vertex whose color you want to copy. The brush, swatch and RGBA
fields update, and the tool returns to painting with that color.

The picker copies the nearest corner's exact authored RGBA bytes. Textures,
fog, selection highlighting, fullbright mode and material preview adjustments
do not alter the sampled color. It uses the same visible-surface picking as
painting, including hidden layers, backface culling, texture holes and object
occlusion. It does not sample colors from placed models or the desktop.

A visible Stan tile can also supply its stored RGB; the brush keeps its current
alpha because Stan has no alpha channel.

An empty or obstructed click leaves picking active. **Escape** cancels it;
changing tools, changing the scene, starting camera flight or leaving viewport
focus also cancels it. A sampling double-click never paints on its second click.
The button supports Tab, Space and Enter and scrolls into view on keyboard focus.

Sampling only changes the brush: it creates no undo entry and leaves the
document, selection and redo history intact. Subsequent painting uses the
existing paint/undo behavior.

Automated checks: `python3 tools/geditor/tests/vertex_eyedropper/run.py` exercises
source RGBA lookup, brush control updates, real ray/alpha/depth picking,
foreground occlusion, Stan sampling, misses, double-click dispatch and cancellation.
Windows UI smoke check: pick a distinctive color, paint another vertex, undo
the paint, and check the picker's button/HSV layout at a short window height.
