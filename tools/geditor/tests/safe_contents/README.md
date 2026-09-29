# Safe placement and contents

Drag **Safe** from the Objects palette onto a walkable floor. This creates a
native safe (`PsafeZ`) and an unlocked swinging door (`PsafedoorZ`). Both are
selected so the normal move, rotate, and scale tools can position them together.
They remain separate objects; select both again to transform the complete safe.
The initial body is 100 world units tall, using stock safe/door proportions.

To protect a pickup:

1. Place the collectible inside the safe.
2. Select that item and open **Properties > Safe contents...**.
3. Choose the safe body and its door by object index/model, then click **OK**.
4. For a raised item, enable **In Air** and **No fall** in Flags to keep it at
   the intended height. Ordinary props also need **Allow pickup** if they are
   meant to be collected; linking alone does not change their pickup behavior.

Choose **None** for the body (which also clears the door) to remove the link.
Multiple pickups can share one safe. The door's existing key requirements can
be edited separately in its Properties panel. The game gates pickup until the
swing angle exceeds 0.5 degrees, matching stock behavior; it need not reach its
fully open position. The link does not attach or move the item with the safe.

All edits use ordinary setup records, save with the project, and support
undo/redo. No game code change or ROM rebase is needed. Save and Create ROM as
usual to test a level. Do not rely on the raw **Linked to safe** flag alone:
the engine sets it after resolving the native item/body/door link at load time.

Run `python3 tools/geditor/tests/safe_contents/run.py` from the repository root.
The suite uses ASan/UBSan and the real setup parser, compactor, history, model
loader, object placement, and pickup-gate function. It checks the stock model
geometry at four orientations and four level scales, native link offsets amid
non-object commands, persistence, undo/redo, reuse, and rejected edits. It is
not an emulator test.
