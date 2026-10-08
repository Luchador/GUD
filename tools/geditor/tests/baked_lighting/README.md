# Baked Lighting

Open **Tools > Baked Lighting**, immediately after Model Editor. Add individual
rooms or use **Add all rooms**; select entries and use **Remove selected** (or
Delete) to remove them. Empty rooms are omitted. Room lists are saved immediately
in the editor's preferences, separately for each project file and background
asset. Closing the tool, switching levels, and restarting GEditor restore the
appropriate list. Room-number compaction clears that background's saved list to
avoid baking different rooms under reused numbers.

Set ambient/directional RGB (0–255), intensities (0–4), and a world-space vector
pointing **toward** the directional light (+Y up). Vector length does not affect
brightness. The defaults use white light, ambient 0.25, directional 0.75 and a
60-degree smoothing angle.

**Bake Lighting** replaces RGB on both background layers in the listed rooms:

    RGB = clamp(ambientRGB * ambientIntensity * (1 - aoStrength * occlusion)
              + directionalRGB * directionalIntensity * max(dot(normal, light), 0))

Existing RGB is not multiplied again, so repeated identical bakes do not darken
the result. Alpha, UVs, positions, materials and face IDs are retained. This is
ambient and diffuse lighting; directional light does not cast shadows. Save Project
and Create ROM use the normal background pipeline; no engine rebuild or project
rebase is required.

Enable **Ambient occlusion** to darken ambient lighting near other surfaces.
**Strength** ranges from 0% (no effect) to 100%. **Radius** is measured in world
units, defaults to 200, and must be positive. Nearby hits have a stronger effect,
falling off to zero at the radius. The calculation uses all primary BG faces,
including rooms outside the bake list; secondary BG and objects do not block it.
Only rooms in the bake list receive new colors, on both BG layers.

AO uses 64 deterministic cosine-weighted hemisphere rays per smoothed vertex
group, with a spatial acceleration tree. Primary triangles act as two-sided
blockers, without sampling texture transparency. AO follows the same hard/smooth
normal rules as the directional light. AO disabled or at 0% preserves the
original bake behavior; repeated identical bakes do not accumulate darkening.

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
editor rollback, undo/redo and saved-state tracking. AO checks cover cross-room
occluders, both receiver layers, world-scale/radius handling, intensity endpoints,
determinism, native persistence and accelerated versus exhaustive tracing. Dialog
tests cover saved room lists across sessions and projects, room renumbering, and
AO checkbox/slider/radius input.
