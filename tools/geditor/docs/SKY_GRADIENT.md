# Sky gradient

Rebuild **GUD and GEditor**, then rebase the project onto the rebuilt ROM.
Open **Settings > Level Settings > Environment**, choose the environment row,
and select **Sky gradient** in the Section list.

- Enable sky gradient to replace the solid backdrop.
- The existing fog/sky RGB remains the horizon color and continues to drive fog,
  distant-prop blending and the water's horizon color.
- Zenith RGB is the color overhead. The first enable initializes it to a darker
  version of the current horizon; subsequent toggles retain the authored values.
- End angle is an elevation from 1 to 90 degrees. The default is 90; a smaller
  angle reaches the zenith color sooner. The curve uses smoothstep in the sine
  of elevation, with vertex interpolation between samples.
- Click Apply to preview. Save Project and Create ROM preserve the settings.
- Choose Alternate to author the destination of a scripted sky/fog transition.
  Enabled/disabled endpoints fade between the gradient and a solid sky.

The gradient is independent of clouds and fog enable flags. It follows world
up, camera pitch/roll, FOV and the existing horizon offset; camera translation
does not move it. Water and clouds render over it, followed by the sun/moon and
normal world geometry. The sky does not read or write depth.

## Rendering and cost

The native renderer uses a 6-by-8 screen mesh, 63 vertices and 96 untextured
triangles. Two adjacent rows fit the original RSP's 16-vertex cache. Per player
it consumes 1,136 bytes of the dynamic vertex/matrix buffer and about 1 KiB of
display-list commands. There is no gradient texture or texture-pool allocation.
Framebuffer color dithering reduces 16-bit banding. The existing solid/cloud
path is retained for disabled gradients.

Clouds use the existing cloud geometry, UVs and texture. For a gradient they
add only their lit contribution over the backdrop. Contribution is evaluated
at cloud vertices using the local sky color, cloud tint and horizon fade.
This preserves the gradient in black/empty parts of a cloud texture and avoids
an extra cloud mesh. GEditor uses the same color equation in its existing,
more finely sampled cloud preview. The preview remains an approximation of
the game's cloud interpolation and N64 texture filtering.

Very small end angles are limited by mesh sampling and may look softer than
the specified angular interval. Start at 90 degrees and lower it as needed.
Hardware performance and final appearance should be checked in ares/on N64.

## Compatibility

`SkyGradientSettings` appends 12 bytes to `EnvironmentRecord`: enabled u32 at
112, end-angle float at 116, zenith RGB at 120..122 and a reserved byte at 123.
ENVT rows are now 124 bytes. Existing initializers zero-fill the extension and
keep gradients disabled. The manifest advertises the stride automatically.

The editor reads 88-, 104-, 112- and 124-byte ROM layouts, retains all existing
sun/moon fields, and exports each layout at its original stride. Gradient edits
are unavailable on older ROMs until rebase. Project overrides still use named
text fields; their in-memory bitmask is widened to 64 bits for fields 32..36.
Reserved bytes, unrelated rows and the base ROM are preserved.

## Verification

```sh
python3 tools/geditor/tests/environment/run.py
python3 tools/geditor/tests/sky_gradient/run.py
python3 tools/geditor/tests/sky_gradient/pixels.py
python3 tools/geditor/tests/cloud_preview/run.py
python3 tools/geditor/tests/water_preview/run.py
python3 tools/geditor/tests/water_preview/pixels.py
python3 tools/geditor/tests/project_rebase/run.py
```

The pixel tests require Mesa EGL/OpenGL and GL headers, as described in the
scripts. They exercise the production preview, including cloud composition,
background occlusion and restoration of graphics/depth state. The native test
exercises actual mesh construction, memory guards, RSP cache limits, camera
orientation, viewport bounds and matrix restoration. Final full-game visuals
and interactive Windows layout still require testing on the target platforms.
