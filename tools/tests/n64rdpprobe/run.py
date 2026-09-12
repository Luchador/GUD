#!/usr/bin/env python3
"""Check actual production probe lists using the real GBI macros and preflight."""
import os
from pathlib import Path
import shlex
import subprocess
import tempfile

here = Path(__file__).resolve().parent
root = here.parents[2]
with tempfile.TemporaryDirectory(prefix="gud-rdp-probe-") as directory:
    binary = Path(directory) / "check"
    subprocess.run(shlex.split(os.environ.get("CC", "cc")) + [
        "-std=c99", "-O2", "-Wall", "-Wextra", "-Werror", "-no-pie",
        "-DN64_DIAG_RDP_PROBE=1",
        "-Wno-pointer-to-int-cast", "-Wno-int-to-pointer-cast",
        "-I", str(here), "-idirafter", str(root / "include"), "-I", str(root / "src"),
        str(here / "check.c"), str(root / "src/n64rdpcheck.c"),
        "-o", str(binary)], check=True)
    subprocess.run([str(binary)], check=True)
