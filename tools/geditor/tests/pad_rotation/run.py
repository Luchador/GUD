#!/usr/bin/env python3
"""R4300-safe pad directions: exact Depot 152 regression and repeated rotations."""
import os
from pathlib import Path
import subprocess
import tempfile


def main():
    here = Path(__file__).resolve().parent
    src = here.parents[1] / 'src'
    root = src.parents[2]
    shim = here.parent / 'image_import'
    with tempfile.TemporaryDirectory(prefix='geditor-pad-rotation-') as temp:
        work = Path(temp)
        (work / 'setup').mkdir()
        subprocess.run([os.environ.get('CC', 'cc'), '-std=c99', '-O1', '-g', '-Wall', '-Wextra',
                        '-Werror', '-Wno-unused-parameter', '-ffunction-sections', '-fdata-sections',
                        '-fsanitize=address,undefined', f'-I{shim}', f'-I{src}', f'-I{root}',
                        str(here / 'check.c'), str(shim / 'platform.c'),
                        *[str(src / f) for f in ('setupload.c', 'actionblocks.c', 'rotation.c')],
                        '-Wl,--gc-sections', '-lm', '-o', str(work / 'check')], check=True)
        subprocess.run([str(work / 'check'), str(work)], check=True,
                       env=dict(os.environ, ASAN_OPTIONS='detect_leaks=0', UBSAN_OPTIONS='halt_on_error=1'))


if __name__ == '__main__':
    main()
