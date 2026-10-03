# Baked Lighting

Open **Tools > Baked Lighting**, immediately after Model Editor. Add individual
rooms or use **Add all rooms**; select entries and use **Remove selected** (or
Delete) to remove them. Empty rooms are omitted. The window is modeless, and
switching levels clears its room list.

Set ambient/directional RGB (0–255), intensities (0–4), and a world-space vector
pointing **toward** the directional light (+Y up). Vector length does not affect
brightness. The defaults use white light, ambient 0.25, directional 0.75 and a
60-degree smoothing angle.

**Bake Lighting** replaces RGB on both background layers in the listed rooms:

    RGB = clamp(ambientRGB * ambientIntensity
              + directionalRGB * directionalIntensity * max(dot(normal, light), 0))

Existing RGB is not multiplied again, so repeated identical bakes do not darken
the result. Alpha, UVs, positions, materials and face IDs are retained. This is
ambient and diffuse lighting without shadow casting or occlusion. Save Project
and Create ROM use the normal background pipeline; no engine rebuild or project
rebase is required.

Normals are weighted by corner angle and smoothed only through connected,
consistently wound, manifold shared edges within the smoothing angle. Existing
unmerged vertex identities, separate layers, and disconnected fans stay separate.
Sharp corners that need different colors get separate vertex copies, preserving
their other attributes. Degenerate triangles keep their colors and do not affect
normals. Use Merge/Weld to connect an intentionally smooth surface before baking.
Undo before increasing the smoothing angle after a bake that split hard edges.

Every successful bake is one **Undo Bake Lighting** step, including vertex splits.
Invalid inputs, allocation failures and viewport rebuild failures leave the
background unchanged. A no-op bake adds no history step.

Run from the repository root:

    python3 tools/geditor/tests/baked_lighting/run.py

The sanitizer suite checks production lighting and dialog logic, room-list
selection and refresh, hard and smooth shared normals, existing splits, both
layers, alpha/UV/material preservation, degenerates, native compile/save/reload
and ROM batch validation, repeat bakes, allocation/validation failure atomicity,
editor rollback, undo/redo and saved-state tracking.
