#!/usr/bin/env python3
"""Exercise long editing sessions, reference-aware reuse, and native setup/history IO under ASan/UBSan."""
import os
from pathlib import Path
import subprocess
import sys
import tempfile

here = Path(__file__).resolve().parent
src = here.parents[1] / 'src'
shim = here.parent / 'image_import'
with tempfile.TemporaryDirectory(prefix='geditor-setup-lifetime-') as temp:
    work = Path(temp)
    (work / 'setup').mkdir()
    binary = work / 'check'
    subprocess.run([os.environ.get('CC', 'cc'), '-O1', '-g', '-std=c99', '-Wall', '-Wextra',
                    '-Werror', '-Wno-unused-parameter', '-ffunction-sections', '-fdata-sections',
                    '-fsanitize=address,undefined', f'-I{shim}', f'-I{src}', f'-I{src.parents[2]}',
                    str(here / 'check.c'), str(shim / 'platform.c'),
                    *[str(src / name) for name in ('setupload.c', 'actionblocks.c', 'bghistory.c', 'rom.c')],
                    '-Wl,--gc-sections', '-lm', '-o', str(binary)], check=True)
    subprocess.run([str(binary), str(work), *sys.argv[1:]], check=True,
                   env=dict(os.environ, ASAN_OPTIONS=os.environ.get('ASAN_OPTIONS', 'detect_leaks=0'), UBSAN_OPTIONS='halt_on_error=1'))
