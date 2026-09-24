# UV coordinates and cylindrical mapping

In the UV editor's **Move** mode, U and V show the arithmetic mean of the
selected UV vertices. A shared native vertex is counted once. Type a coordinate
and press **Enter** in that box to set that axis on every selected vertex;
the other axis stays unchanged. For example, entering `0.5` in U aligns the
selection vertically in UV space. Dragging the move handles still translates
the group. Rotate and Scale retain their existing angle/factor fields.

Coordinates are normalized: one unit spans the texture's width or height.
Changes are rounded to GoldenEye's native 1/32-texel precision. An entry outside
the signed 16-bit S/T range rejects the entire edit. Invalid text in the other
box does not block a coordinate entry. An unchanged entry adds no undo step.

When the selected faces share one texture, the canvas shows it behind their
UVs. The **Opacity** slider controls the preview inside the outlined 0-1 tile;
repeats outside it use half that opacity. The image repeats in both directions,
including negative coordinates, and stays aligned while panning or zooming.
At 0% both the main image and its repeats disappear.

The **red border** marks the native UV limits, not the 0-1 tile: tiling outside
0-1 is valid. For a texture of width W and height H, the supported range is
`-32768/(32*W)` through `32767/(32*W)` in U, and the equivalent using H in V.
For example, a 32x32 texture permits almost 32 repeats in either direction.
Zoom out or pan towards a limit to see its border. For mixed texture sizes,
the border encloses the range valid for every displayed face. It remains
visible without a shared texture preview and with opacity set to 0%.

## Planar mapping at a fixed texel size

**Use texel size** sits between **Axis** and **Texel size**. It starts unchecked
and retains its state while GEditor is running. When checked, **X-axis**,
**Y-axis**, **Z-axis**, and **Best Fit** use the entered physical texel size
instead of fitting the selected faces into 0-1.

The existing **Texel size (cm)** value is centimeters per texel, the reciprocal
of texels/cm. For example, **4** means 0.25 texels/cm: a 32 x 32 image repeats
every 128 cm. Texture width and height affect the number of UV repeats; the
physical texel scale is the same in both directions, including rectangular
images and selections with different texture dimensions. BG positions include
room origins and level scale. Model UVs use the model's units, as indicated
by the field label.

Projection uses all displayed faces. The projected minimum starts at U=V=0;
Best Fit measures distances on its fitted plane. Angled faces can still be
foreshortened by planar projection. Unchecking the box restores the existing
uniform fit centered in 0-1, and ignores the size field for planar operations.
Changing the checkbox alone does not edit UVs. Each mapping uses normal
Undo/Redo and Save Project. Out-of-range mappings are rejected together;
increase Texel size or project a smaller selection to fit native S/T limits.

## Cylindrical unwrap

Select the complete **uncapped side wall** of one cylinder in face mode, open
Tools > UV Editor, choose **Axis** and **Texel size (cm)**, then click
**Cylindrical**. The operation uses all faces displayed in the UV Editor,
independently of the selected UV vertices.
It always uses Texel size; the checkbox controls planar mapping only.

- **Auto** derives the axis from the two open rims, using their area centroids
  and rim planes to handle both tall and short cylinders.
  Uneven tessellation, rotated tubes, short/wide cylinders and polygonal or
  elliptical cross-sections are supported. X/Y/Z explicitly choose a world
  axis; an upright smokestack uses **Y**.
- **Texel size (cm)** is world centimetres covered by one image pixel, in both
  directions. The default is 4. A 32 x 32 texture therefore repeats every
  128 cm. Larger values make the texture larger; smaller values add repeats.
  Image dimensions are respected when the canvas converts native texels to UVs.
- U measures distance along the polygonal rim; V measures height along the
  chosen axis. Adjacent wall faces remain connected in UV space except at
  the seam. The unwrap does not stretch the whole wall into a square 0-1 tile.
- Cylindrical expects one wall with two open rims. Caps, non-manifold geometry,
  disconnected walls and invalid seam paths are rejected without changing UVs.
  Strong bends or tapering still introduce projection distortion; this tool
  is intended for approximately straight cylinders, not a general relax solver.

### Marking the seam

1. Switch the viewport to **Edge** mode (2).
2. Right-click an edge and choose **Mark Seam**. For a subdivided wall, mark
   a continuous edge path from one open rim to the other. Do not mark the rims.
3. Marked edges appear **green while the UV Editor is open**. Right-click a
   marked edge and choose **Clear Seam** to remove its mark.
4. Return to **Face** mode (3), select the whole wall, and click Cylindrical.

A marked seam must be one connected, unbranched path. Incomplete, multiple,
branched, closed or rim-following seams produce an explanation. With no marks
on the selection, the tool chooses a connected edge path automatically.
Seam marking does not split or move geometry. Both sides of matching geometric
edges are marked even if their native vertices were split by UVs or colors.

Seam marks support undo/redo and Save Project. They are stored beside each BG
as an editor-only `.uvseams` file, using geometric edge keys that survive native
vertex compaction and UV splitting. They do not enter the ROM. Face winding,
edge bisection, extrusion and knife cuts retain the applicable seam portions.
The normal project rebase copy also carries these guide files forward; keys
that no longer match geometry are ignored when loading.

Mapping creates native vertex copies only where different UVs require them.
Copies retain position, color and flags; unselected faces retain their UVs.
Each mapping is one undo step and saves through the normal background compiler.
If the result exceeds native S/T limits, the entire edit is rejected; increase
Texel size to reduce the number of repeats.

## Verification

Run `python3 tools/geditor/tests/uv_cylinder/run.py`. The native tests use
AddressSanitizer and UndefinedBehaviorSanitizer and cover:

- 396 regular cylinder cases, including 6-16 sides, three aspect ratios,
  rotation, duplicated native vertices and integral coordinate rounding.
- Explicit seams on every axis, irregular elliptical rims, multiple height
  segments, diagonal seam paths, shared-edge continuity and rejected seams/caps.
- Per-corner UV splitting, colors/materials, native compile/save/reload,
  undo/redo, repeated mapping, rollback and native coordinate limits.
- Seam guide persistence, winding, marking across native splits, failed file
  replacement and confirmation that marks do not change compiled BG bytes.

Texture canvas and existing topology tests remain in `tests/uv_texture`,
`tests/edge_extrusion`, `tests/bisect_edge` and `tests/knife`.
