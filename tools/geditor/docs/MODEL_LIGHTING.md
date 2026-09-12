# Lit editor models

The startup box and setup marker GLBs share `modellighting.c`. Their existing
vertex colors, glTF base-color tints, and transformed normals feed a GLSL 1.20
Phong shader. Normals are interpolated and normalized per fragment; diffuse
and specular lighting are evaluated in eye space. Back-facing surfaces use
flipped normals for two-sided lighting. Highlights retain an independent
color instead of being multiplied by the blue diffuse material.

`ModelLightingSettings` provides ambient and diffuse light, a directional
light vector, specular material color/strength, and shininess. The initial
settings use a white specular strength of 0.65 and a Phong exponent of 32.
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
40 degrees per second (one turn every 9 seconds). The framing reserves space
for the complete rotation even in narrow windows.
