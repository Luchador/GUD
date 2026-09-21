#!/usr/bin/env python3
"""Conservative geometry and real native setup IO under ASan/UBSan."""
import os
from pathlib import Path
import subprocess
import tempfile

here = Path(__file__).resolve().parent
src = here.parents[1] / 'src'
root = src.parents[2]
shim = here.parent / 'image_import'
with tempfile.TemporaryDirectory(prefix='gud-occluders-') as folder:
    work = Path(folder)
    (work / 'setup').mkdir()
    flags = [os.environ.get('CC', 'cc'), '-O1', '-g', '-std=c99', '-Wall', '-Wextra',
             '-Werror', '-Wno-unused-parameter', '-ffunction-sections', '-fdata-sections',
             '-fsanitize=address,undefined', f'-I{shim}', f'-I{src}', f'-I{root}']
    for name, sources in (
        ('math', []),
        ('editor', [shim / 'platform.c', *[src / f for f in (
            'setupload.c', 'actionblocks.c', 'bghistory.c', 'rotation.c', 'scaling.c',
            'setupstan.c', 'stanquery.c', 'occluders.c')]])):
        binary = work / name
        subprocess.run(flags + [str(here / f'{name}.c'), str(root / 'src/game/occlusionmath.c'),
                       *map(str, sources), '-Wl,--gc-sections', '-lm', '-o', str(binary)], check=True)
        subprocess.run([str(binary), str(work)], check=True,
                       env=dict(os.environ, ASAN_OPTIONS='detect_leaks=0', UBSAN_OPTIONS='halt_on_error=1'))
