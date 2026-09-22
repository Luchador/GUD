#!/usr/bin/env python3
"""Live scale changes using real BG/STAN/history code; optional native BG fixture."""
import os
from pathlib import Path
import subprocess
import sys
import tempfile
here = Path(__file__).resolve().parent
src = here.parents[1] / 'src'
with tempfile.TemporaryDirectory(prefix='geditor-scales-') as folder:
    binary = Path(folder) / 'check'
    sources = ('levelscale.c', 'bghistory.c', 'bgdocument.c', 'bgload.c', 'bgmaterial.c',
               'bgrender.c', 'stanload.c', 'setupload.c', 'actionblocks.c')
    command = [os.environ.get('CC', 'cc'), '-std=c99', '-O1', '-g', '-Wall', '-Wextra', '-Werror',
               '-Wno-unused-parameter', '-ffunction-sections', '-fdata-sections', '-fsanitize=address,undefined',
               f'-I{here.parent / "image_import"}', f'-I{src}', f'-I{src.parents[2]}', str(here / 'check.c'),
               str(here.parent / 'image_import/platform.c'), *[str(src / s) for s in sources],
               '-Wl,--gc-sections', '-lm', '-o', str(binary)]
    subprocess.run(command, check=True)
    subprocess.run([str(binary), *sys.argv[1:]], check=True,
                   env=dict(os.environ, ASAN_OPTIONS='detect_leaks=0', UBSAN_OPTIONS='halt_on_error=1'))
