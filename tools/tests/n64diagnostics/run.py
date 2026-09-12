#!/usr/bin/env python3
"""Run the production diagnostic code with host hardware/OS shims."""
import os
from pathlib import Path
import shlex
import subprocess
import sys
import tempfile

here = Path(__file__).resolve().parent
root = here.parents[2]
with tempfile.TemporaryDirectory(prefix="gud-n64diag-") as directory:
    binary = Path(directory) / "check"
    subprocess.run(shlex.split(os.environ.get("CC", "cc")) + [
        "-std=c99", "-O2", "-Wall", "-Wextra", "-Werror",
        "-Wno-pointer-to-int-cast", "-Wno-int-to-pointer-cast", "-Wno-unused-parameter",
        "-I", str(here), "-I", str(root / "src"), str(here / "check.c"),
        str(root / "src/n64rdpcheck.c"),
        str(here / "rdpcheck.c"),
        "-o", str(binary)], check=True)
    subprocess.run([str(binary), *sys.argv[1:]], check=True)
