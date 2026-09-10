#!/usr/bin/env python3
"""Check current project/ROM layouts and BG validation with production code."""
import os
from pathlib import Path
import re
import subprocess
import tempfile


def function(source, name):
    match = re.search(r"^(?:static )?(?:BOOL|DWORD|void) " + name + r"\(", source, re.M)
    if match is None:
        raise RuntimeError(f"Missing production function: {name}")
    start = source.index("{", match.start())
    depth, end = 1, start + 1
    while depth:
        depth += (source[end] == "{") - (source[end] == "}")
        end += 1
    return source[match.start():end]


def main():
    here = Path(__file__).resolve().parent
    src = here.parent.parent / "src"
    shim = here.parent / "image_import"
    source = (src / "romexport.c").read_text()
    # Exercise exact metadata-export functions without the Windows file picker
    # and unrelated resource relocation paths.
    export = '#include <stdio.h>\n#include <stdlib.h>\n#include <string.h>\n#include <stdarg.h>\n'
    export += '#include "romexport.h"\n#define ROM_KIND_STGT 0x53544754u\nstatic char g_RomExportError[256];\n'
    for name in ("RomExportSetError", "RomExportRead32", "RomExportWrite32", "RomExportWrite16",
                 "RomExportBasePath", "RomExportProjectMatchesRom", "RomExportRefreshProjectLevelMetadata",
                 "RomExportUpdateLevelTable"):
        export += function(source, name) + "\n"
    export += 'BOOL TestUpdateLevelTable(const GEditorProject *p, RomFile *r, const char **why) '
    export += '{ return RomExportUpdateLevelTable(p, r, why); }\n'
    with tempfile.TemporaryDirectory(prefix="geditor-current-formats-") as temp:
        work = Path(temp)
        (work / "export.c").write_text(export)
        command = [os.environ.get("CC", "cc"), "-O1", "-g", "-std=c99", "-Wall", "-Wextra",
                   "-ffunction-sections", "-fdata-sections", "-fsanitize=address,undefined",
                   "-Dfopen=TestFopen", f"-I{shim}", f"-I{src}", str(here / "check.c"),
                   str(shim / "platform.c"), str(work / "export.c")]
        command += [str(src / name) for name in ("rom.c", "project.c", "bgcompile.c")]
        command += ["-Wl,--gc-sections", "-lm", "-o", str(work / "check")]
        subprocess.run(command, check=True)
        env = dict(os.environ, ASAN_OPTIONS="detect_leaks=0", UBSAN_OPTIONS="halt_on_error=1")
        subprocess.run([str(work / "check"), str(work)], env=env, check=True)


if __name__ == "__main__":
    main()
