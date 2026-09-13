# Viewport rendering styles

Choose a style in **View**, below **Fog**. The marked entry is the active style.

| Style | Shortcut | Display |
| --- | --- | --- |
| Normal | Ctrl+1 | Existing textures and vertex colors. |
| Wireframe | Ctrl+2 | Normal shading with white triangle edges over background, objects and characters. |
| Fullbright | Ctrl+3 | White vertex RGB, preserving alpha and textures. |
| Untextured | Ctrl+4 | Vertex colors with texture sampling disabled. |

These styles affect only the main viewport. They do not change assets or mark
the project dirty. Normal restores the usual display. Fog and visibility remain
independent controls; turn Fog off separately if you want an unfogged Fullbright
view. Selection highlights, collision/portal guides and other editing aids keep
their identifying colors. The separate model editor retains its normal preview.

Wireframe reuses the vertex/edge tools' overlay, so switching to those tools does
not double the background lines. Vertex markers still appear in vertex mode.
Hidden faces and disabled geometry layers stay hidden.

Fullbright preserves vertex alpha, including alpha used by cutout and translucent
materials. Untextured removes texture alpha with the texture; vertex alpha still
applies. Picking follows the resulting solid surfaces instead of clicking through
holes from an invisible texture.

Ctrl+1–4 also work during camera flight. Property text fields and floating editors
keep their own keyboard input. The style persists while the editor is open,
including across level changes, and starts at Normal on a fresh launch.

Host regression checks: `python3 tools/geditor/tests/view_modes/run.py`.
