# Rebase a project onto a newer GUD ROM

Use **File → Rebase Project…** when you want an existing project to use a new
GUD build, such as one containing character or rendering optimizations.

1. Open the existing project.
2. Choose the new GUD ROM, a new project name, and its destination directory.
   If the project was created from an exported ROM containing replaced stock
   textures, leave **Keep existing base images** checked to carry them forward.
   **Level file conflicts** and **Model conflicts** default to **Stop on conflicts**.
3. Select **Save and Check**. This saves the current project's edits and checks
   the retained `base.z64`, the incoming ROM, and the saved assets.
4. Review the result, then select **Create Copy**. GEditor creates and opens the
   new project. Use **Tools → Create ROM…** in that project to build the playable ROM.

The original project folder remains available with its original base ROM.
Canceling before Save and Check does not save edits. Save and Check performs a
normal project save, even if the subsequent compatibility check finds a conflict.

## What carries over

Unless explicitly replaced using the conflict choices, the new project retains saved background geometry and materials, portals, setup
objects and characters, stan geometry, native model replacements, imported and
replaced images, deleted-image records, previews, and other project sidecars.
Image IDs, conversion settings, and remembered source paths are preserved. A
missing original BMP source does not block rebasing if the project's saved
image and native record are intact; a later Reimport still needs that source.
External files referenced by absolute paths are not relocated or copied.

When **Keep existing base images** is unchecked, shared base image IDs must
contain identical native textures and settings. The checked option chooses the project's old
base for every differing shared image ID. This retains replacements baked into
an exported ROM, including dimensions, pixel format, alpha, mipmaps, and native
surface/detail settings. Saved project image edits still apply on top. The
option starts checked; changing it requires another **Save and Check**.
The report lists the IDs being kept before **Create Copy** is enabled.

Different base image counts are also supported:

- If the incoming ROM has fewer images, the old base's extra images are carried
  into the new project's base at their existing IDs. This includes a lamp
  texture baked into a previously exported ROM. Native surface and detail flags
  are preserved, and BG/model/material references keep the same IDs.
- If the incoming ROM has more images, new slots are adopted. A slot already
  used by a project import must match that import's effective native data and
  settings, including any saved BMP edits. Different contents at the same ID
  are reported as a conflict; neither texture is silently replaced.
- Saved image metadata is validated against the old base before its base
  fingerprint is updated in the new copy. Import settings, source paths,
  deletion records and BMPs survive. Newly added stock images receive previews.

The compatibility report lists carried-forward base images and incoming image
slots. The selected incoming ROM file is never modified.

ROM addresses can move. GEditor discovers the new locations through the new
ROM's manifest and resource table, and exports edits using those locations.
It does not copy old executable code into the new ROM.

For each saved BG, setup, and stan resource, GEditor compares the old base,
project copy, and incoming base:

| Change | Result |
| --- | --- |
| Only the project changed | Keep the project's file |
| Only the incoming ROM changed | Update the project copy from the new ROM |
| Both contain the same change | Accept the shared result |
| Both changed differently | Apply the selected conflict choice; stop by default |

**Level file conflicts** applies to saved BG, setup, stan and text-bank files.
**Keep project** retains each conflicting project file; **Use new ROM** replaces
each conflicting file with the incoming version. These are whole-file choices,
not geometric merges. The report names every resolved file before Create Copy.
They do not override conflicts in level settings, environment fields or catalogs.

Native character, prop and weapon models follow the same rule, independently
controlled by **Model conflicts**. Changed stock models with no competing saved
native edit update automatically. Incoming models receive fresh editable glTF
previews, and their thumbnail cache refreshes when used. Choosing the incoming
model removes any superseded `.gmodel` override from the new copy.

Retained model overrides are validated against the old base, then rebound to
the new base fingerprint. Their native data and editable glTF metadata remain
intact. If the project model already matches the incoming model, it is retained
without a conflict, including its editor metadata. Differences of up to 15 zero
alignment bytes are ignored for this comparison. Invalid native models or stale
override fingerprints still stop the operation, regardless of the selected choice.

For a code/model upgrade where the saved levels are authoritative, choose
**Keep project** for level conflicts. Leave model conflicts on **Stop on conflicts**
to review competing model edits, or choose **Use new ROM** when those models
should be replaced with the incoming versions. Nonconflicting assets still use
the automatic three-way comparison. Changing either choice requires another check.

A missing local resource override inherits the incoming ROM resource during
ROM export. Render scale, music, background sound, and the extra music track
use the same three-way rule. Environment overrides merge per field: untouched
defaults refresh from the new ROM, while conflicting edits or missing edited
rows block the rebase. Level names refresh from the new ROM. An incoming world
coordinate scale change is rejected because it
requires coordinated geometry conversion.

## Removed unused levels

New GUD builds omit SHO, ELD, LUE, RIT, EAR, LEE, LIP, WAX and PAM. Rebase
removes those placeholder rows from the new project's level list. Unedited
local copies of their retired BG/stan/setup resources are removed from the
new copy; resources shared with surviving levels remain available under their
surviving names. The compatibility report includes the removal counts.

Saved edits to a retired level's settings or resource block the rebase and
identify the affected entry. An asset without an old base for comparison also
blocks removal. Nonempty retired language banks require manual migration.
The original project remains available with all of its files.

The ROM file table is compacted, so its runtime indices can change. GEditor
matches complete, unique resource names across the two ROMs and uses the new
locations when exporting. Stage IDs, model IDs, image IDs and encoded text-bank
IDs retain their values. This preserves setup, environment and model references.

## Compatibility limits

Rebasing is intended primarily for code and compatible asset updates. Both
ROMs must use the current GUD manifest format, with matching surviving stage
IDs and resource names, compatible model catalogs, and compatible texture banks
as described above. Native model contents may change. File-table indices
may change; every named row must still have the correct index and a unique
name. Unexpected resource additions/removals and removal of other stages are
rejected. Other changed resource types without a merge
schema are rejected too. It does not remap model IDs or texture IDs, migrate
older project/ROM formats, or merge conflicting binary geometry automatically.

Changing a shared base image is rejected unless **Keep existing base images**
is selected. That option keeps the old ID-to-texture mapping; it does not migrate
an incoming build that inserts/reorders texture IDs or rewrites their references.
Conflicting imported IDs require separate migration; the rebase does not guess
new IDs or rewrite arbitrary texture references. Corrupt or missing image
metadata still blocks the operation.

Conflicts identify the affected resource or setting in the dialog. Use the
asset choices for competing files; other compatibility issues require a project
correction or a compatible GUD build, then another check.

## Failure handling

Create Copy repeats compatibility checks against a temporary copy of the saved
project. It performs a complete ROM export in memory, including native asset
validation and checksum generation, before publishing the destination folder.
No playable ROM is written by that validation step.

An existing destination is never replaced. Destination folders inside the
source project, including junction aliases, are refused. Linked files and
folders within the project are refused rather than followed. Copy, save,
validation, and publication failures remove the temporary copy; if cleanup is
blocked, the error identifies the remaining temporary folder. The source
project is not changed by the copy/rebase step.

## Development checks

From the repository root on Linux/WSL:

```sh
python3 tools/geditor/tests/project_rebase/run.py
# Optional: exercise a compatible pair of local ROMs with changed models.
python3 tools/geditor/tests/project_rebase/run.py --base-rom /path/to/old.z64 --rom /path/to/new.z64
make -C tools/geditor/src
```

The regression test needs a C compiler with AddressSanitizer and
UndefinedBehaviorSanitizer. It uses synthetic current-format ROMs, the
repository's Jungle tree model, actual image/model export code, and a POSIX
shim for Windows file operations. It covers relocated tables, preserved edits,
three-way merging, explicit file/model choices, model fingerprint migration,
automatic stock-model updates, compatibility conflicts, missing/corrupt assets, repeated
rebasing, larger/smaller base image banks, imported-ID collisions, retained
texture flags, the explicit choice to keep differing base images (formats,
dimensions, data and settings), preview generation, removal of the nine unused stages and their
26 file records, shifted file indices, retained shared assets, removal conflicts,
and copy/write/publication failures. The optional ROM-pair test extracts real
images/resources, rebases, reopens and exports, then compares every adopted model
with the incoming native bytes and validates the refreshed glTF source identities.
It does not exercise the Windows
dialog or run the exported fixture on an N64.
