#!/usr/bin/env python3
"""Test production accounting, scheduler hooks and HUD with OS/hardware shims."""
import os
from pathlib import Path
import shlex
import re
import subprocess
import tempfile

here = Path(__file__).resolve().parent
root = here.parents[2]

def function(source, name):
    """Copy an actual top-level definition; do not duplicate scheduler logic."""
    match = re.search(r"^(?:static )?(?:void |Gfx \*)" + name
                      + r"\([^\n]*\)\s*\n\{[\s\S]*?^}", source, re.M)
    if not match:
        raise RuntimeError("Cannot find production function: " + name)
    return match.group(0)

with tempfile.TemporaryDirectory(prefix="gud-rcpprofile-") as temporary:
    binary = Path(temporary) / "check"
    subprocess.run(
        shlex.split(os.environ.get("CC", "cc"))
        + ["-std=c99", "-Wall", "-Wextra", "-Werror", "-O2",
           "-I", str(here), "-I", str(root / "src"),
           str(here / "check.c"), str(root / "src/rcpprofile.c"),
           "-o", str(binary)], check=True
    )
    subprocess.run([str(binary)], check=True)

    hud = (root / "src/game/lv.c").read_text()
    Path(temporary, "hud_functions.inc").write_text("\n".join(
        function(hud, name) for name in
        ("lvDrawProfilerText", "lvDrawFrameRateDisplay")))
    binary = Path(temporary) / "hud"
    subprocess.run(shlex.split(os.environ.get("CC", "cc")) + [
        "-std=c99", "-Wall", "-Wextra", "-Werror", "-O2",
        "-I", str(here), "-I", str(root / "src"), "-I", temporary,
        str(here / "hud.c"), str(root / "src/rcpprofile.c"),
        "-o", str(binary)], check=True)
    subprocess.run([str(binary)], check=True)

    # Use the production flag/register constants as well as the entry points.
    scheduler = (root / "src/sched.c").read_text()
    constants = []
    for path in (root / "src/sched.c", root / "src/sched.h", root / "include/PR/rcp.h"):
        constants.extend(line for line in path.read_text().splitlines()
                         if re.match(r"#define\s+(OS_SC_|DPC_)", line))
    Path(temporary, "scheduler_constants.inc").write_text("\n".join(constants))
    Path(temporary, "scheduler_functions.inc").write_text("\n".join(
        function(scheduler, name) for name in
        ("__scHandleRSP", "__scHandleRDP", "__scExec", "__scYield")))
    binary = Path(temporary) / "scheduler"
    subprocess.run(shlex.split(os.environ.get("CC", "cc")) + [
        "-std=c99", "-Wall", "-Wextra", "-Werror", "-Wno-unused-but-set-variable", "-O2",
        "-I", str(here), "-I", str(root / "src"), "-I", temporary,
        str(here / "scheduler.c"), str(root / "src/rcpprofile.c"),
        "-o", str(binary)], check=True)
    subprocess.run([str(binary)], check=True)
