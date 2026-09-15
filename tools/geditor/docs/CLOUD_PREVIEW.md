# Cloud preview

Opening a level with clouds enabled now shows an animated cloud sky behind
the geometry. The preview uses that level's sky colour, cloud colour, height
and horizon offset, plus the cloud image from the project's Images collection.
Replacing that image refreshes the preview immediately.

Clouds fade into the sky colour near the horizon and move slowly. Normal,
Wireframe and Fullbright views show them; Untextured view keeps a solid
background. The model editor and startup model have no cloud preview.

The sky is an approximation of `sky.c`, with a screen mesh sampling a
horizontal cloud plane. Its geometry cannot be selected or edited. It draws
behind level geometry without writing depth or changing triangle statistics.
It does not preview water, weather or the exact N64 filtering.

Cloud metadata is read from the base ROM's existing `ENVT` records. Project
reopen and rebase refresh it automatically; no project or ROM format change
is needed. Missing cloud images or unsupported settings leave the solid sky
background in place. The image bank mapping follows `s_skywaterimages` in
`assets/oddtextures.c`.

Checks from the repository root:

```sh
python3 tools/geditor/tests/cloud_preview/run.py
python3 tools/geditor/tests/current_formats/run.py
python3 tools/geditor/tests/project_rebase/run.py
```

These exercise cloud sampling and tint, environment selection and validation,
project reopen, rebase and ROM export. The Windows build checks the viewport
integration. Visual review should include Dam or Depot, both Surface levels,
a level with clouds disabled, looking up/down, geometry edits, and replacing
the cloud image. The OpenGL pass was also checked offscreen for horizon fade,
animation, foreground occlusion, untouched depth and restored GL state.
