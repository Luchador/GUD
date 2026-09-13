# Rebase a project onto a newer GUD ROM

Use **File → Rebase Project…** when you want an existing project to use a new
GUD build, such as one containing character or rendering optimizations.

1. Open the existing project.
2. Choose the new GUD ROM, a new project name, and its destination directory.
3. Select **Save and Check**. This saves the current project's edits and checks
   the retained `base.z64`, the incoming ROM, and the saved assets.
4. Review the result, then select **Create Copy**. GEditor creates and opens the
   new project. Use **File → Create ROM…** in that project to build the playable ROM.

The original project folder remains available with its original base ROM.
Canceling before Save and Check does not save edits. Save and Check performs a
normal project save, even if the subsequent compatibility check finds a conflict.

## What carries over

The new project retains saved background geometry and materials, portals, setup
objects and characters, stan geometry, native model replacements, imported and
replaced images, deleted-image records, previews, and other project sidecars.
Image IDs, conversion settings, and remembered source paths are preserved. A
missing original BMP source does not block rebasing if the project's saved
image and native record are intact; a later Reimport still needs that source.
External files referenced by absolute paths are not relocated or copied.

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
| Both changed differently | Report a conflict and stop |

A missing local resource override inherits the incoming ROM resource during
ROM export. Render scale, music, background sound, and the extra music track
use the same three-way rule. Level names and environment metadata refresh from
the new ROM. An incoming world coordinate scale change is rejected because it
requires coordinated geometry conversion.

## Compatibility limits

This first version is intended primarily for code optimization updates. Both
ROMs must use the current GUD manifest format, with matching level/resource IDs
and names, compatible model catalogs, unchanged native model resources, and an
unchanged original texture bank. Other changed resource types without a merge
schema are rejected too. It does not remap model IDs or texture IDs, migrate
older project/ROM formats, or merge conflicting binary geometry automatically.

Changing the base image count is rejected: a newly added stock image could
otherwise take an ID already used by an imported project image. Native model
and image fingerprints are validated, not bypassed or rewritten.

Conflicts identify the affected resource or setting in the dialog. Resolve them
in a project copy, or use a compatible GUD build, then check again.

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
make -C tools/geditor/src
```

The regression test needs a C compiler with AddressSanitizer and
UndefinedBehaviorSanitizer. It uses synthetic current-format ROMs, the
repository's Jungle tree model, actual image/model export code, and a POSIX
shim for Windows file operations. It covers relocated tables, preserved edits,
three-way merging, compatibility conflicts, missing/corrupt assets, repeated
rebasing, and copy/write/publication failures. It does not exercise the Windows
dialog or run the exported fixture on an N64.
