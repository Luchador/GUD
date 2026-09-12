# Lit editor models

The startup box and setup marker GLBs share `modellighting.c`. Their existing
vertex colors, glTF base-color tints, and transformed normals feed a GLSL 1.20
Phong shader. Normals are interpolated and normalized per fragment; diffuse
and specular lighting are evaluated in eye space. Back-facing surfaces use
flipped normals for two-sided lighting. Highlights retain an independent
color instead of being multiplied by the blue diffuse material.

`ModelLightingSettings` provides ambient and diffuse light, a directional
light vector, specular material color/strength, and shininess. In
`modellighting.c`, `g_ModelLightingDefaults` supplies other editor models
(including setup markers) with direction `{-0.4f, 0.8f, -0.6f, 0}`.
`g_StartupModelLighting` independently preserves the box's direction
`{0.25f, -0.5f, 0.7f, 0}`. Both currently use ambient strength 0.4, diffuse
strength 0.8, white specular strength 0.8, and a Phong exponent of 15.
Passing `NULL` to `ModelLightingBegin` selects `g_ModelLightingDefaults`.
Call `ModelLightingBegin` after establishing the view matrix and before the
model transform; pair it with `ModelLightingEnd`. Callers save and restore
fixed-function attributes. The module separately restores the previous shader
program and owns its program for one OpenGL context. A future scene renderer
can pass its own settings through this same API.

If the GL context cannot compile the shader, models use fixed-function
specular lighting. This fallback evaluates lighting at vertices, while the
shader evaluates it per pixel. Existing level BG and native game model
rendering continue through their authored material/vertex-color paths.

The startup camera looks down 30 degrees and aims slightly above the box's
pivot, placing it lower in the viewport. The box turns counter-clockwise at
the rate set by `VIEWPORT_STARTUP_SPIN_DEGREES_PER_SECOND` in `viewport.c`
(currently 75 degrees per second). The framing reserves space
for the complete rotation even in narrow windows.
