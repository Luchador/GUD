# Occluder experiment

Rebuild GUD and GEditor with this patch, then rebase the project onto that GUD
ROM. Existing projects have no occluders until you place them. You can author
boxes before rebasing, but exporting them requires the updated runtime.

## Authoring

1. Open **Objects > Special** and drag **Occluder** onto a level surface.
2. Select its cyan wire box. Use **W** to move, **E** to rotate, and **R** to
   scale, using the handles or transform fields. The initial box is 100 game
   world units per side; its origin is at the middle of the bottom face.
3. Fit the entire box inside permanent, opaque level geometry. Leave a small
   inset from the visible surface. For Depot, use a box inside each solid
   warehouse crate stack only if that stack cannot move or be destroyed.
4. Save and create a ROM normally. Delete removes a selected box; edits support
   undo/redo. Boxes remain visible and selectable through geometry in GEditor.

Boxes are independent of STAN floors and never snap down to the floor. They do
not create collision, block guards' vision, or render a surface in the game.
They should not be used as gameplay pads. Up to 32 boxes may exist per setup.
Single-player and multiplayer setups author their boxes separately.

## Scope and testing

This first pass skips drawing fully covered, standalone generic props with a
single model matrix and no animation or attached child props. It leaves
characters, doors, pickups, articulated models, effects and background rooms
on their existing rendering paths. It also preserves prop ticking, matrix
updates, targeting, collisions, AI and onscreen flags. Savings are primarily
model display-list submission and RSP/RDP work.

The test encloses the whole model in a conservative sphere, including
non-uniform object scaling. A sphere must be wholly inside the shadow cast by
one camera-facing box face. Partial cover, touching edges, near-clipped faces,
invalid volumes and cameras inside a box fail open. Separate boxes' coverage
is not combined, so some completely hidden props may still draw.

During single-player gameplay:

- **D-pad Up** opens the existing X-ray debug view.
- **D-pad Left** while X-ray is open toggles occlusion. The HUD shows the number
  of boxes and skipped prop render calls for the current view. Opaque and
  translucent passes can count the same prop separately.
- **D-pad Down** closes X-ray and retains the chosen occlusion setting. This
  lets you compare normal rendering/FPS from the same position. Reloading a
  level turns occlusion back on.

Walk around each stack and check all edges, close approaches, elevated views,
and any destructible nearby props. Compare the same view with occlusion on and
off. Boxes reserve 340 bytes each from the stage pool (rounded to 16-byte
alignment): two boxes use 688 bytes; the 32-box maximum uses 10,880 bytes. No
box storage is allocated for levels without boxes, and no Expansion Pak bank
is required. No hardware performance improvement is assumed until measured.

## Format and reversibility

An occluder is a dedicated native 68-byte `BoundPadRecord`. The on-disk STAN
pointer word at offset 40 contains `GEOC` (`0x47454f43`); the non-null `plink`
points to an empty string. Position, up/look axes and six extents use the normal
bound-pad layout. The runtime converts position/extents into world units,
copies the volume into the stage pool and clears the marker. These pads are
excluded from STAN refresh/unresolved-pad reports. Deletion keeps a stable
pad-index tombstone; safe recreation reuses it. Object and script command
indices do not change.

The `OCCL` manifest capability, version 1, identifies compatible runtimes.
Save, compaction, ROM resource repacking and rebase preserve the marker and
dimensions. Export rejects unsupported runtimes and invalid/excess boxes;
rebase checks capabilities against its destination ROM.

To abandon the experiment, delete the boxes and save, then reverse the patch
with `git apply -R GUD-editor-occluders.patch`. Keep a project backup when
switching editor/runtime versions.

Automated checks: `python3 tools/geditor/tests/occluders/run.py` covers geometry
against independent ray/box tests and real setup/history IO. The existing
project-rebase test also covers capability upgrades, export and repeat rebase.
