#!/usr/bin/env python3
"""Exercise unused-pad deletion and native setup/history IO under ASan/UBSan."""
import os
from pathlib import Path
import subprocess
import tempfile

here = Path(__file__).resolve().parent
src = here.parents[1] / 'src'
shim = here.parent / 'image_import'
with tempfile.TemporaryDirectory(prefix='geditor-pad-deletion-') as temp:
    work = Path(temp)
    (work / 'setup').mkdir()
    binary = work / 'check'
    subprocess.run([os.environ.get('CC', 'cc'), '-O1', '-g', '-std=c99', '-Wall', '-Wextra',
                    '-Werror', '-Wno-unused-parameter', '-ffunction-sections', '-fdata-sections',
                    '-fsanitize=address,undefined', f'-I{shim}', f'-I{src}', f'-I{src.parents[2]}',
                    str(here / 'check.c'), str(shim / 'platform.c'),
                    *[str(src / name) for name in ('setupload.c', 'actionblocks.c', 'bghistory.c')],
                    '-Wl,--gc-sections', '-lm', '-o', str(binary)], check=True)
    subprocess.run([str(binary), str(work)], check=True,
                   env=dict(os.environ, ASAN_OPTIONS='detect_leaks=0', UBSAN_OPTIONS='halt_on_error=1'))
