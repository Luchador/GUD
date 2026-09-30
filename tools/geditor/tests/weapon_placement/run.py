#!/usr/bin/env python3
"""Native weapon pickup placement, model/type changes, save/reload and history."""
import os
from pathlib import Path
import runpy
import subprocess
import tempfile


def main():
    here = Path(__file__).resolve().parent
    src = here.parents[1] / 'src'
    root = src.parents[2]
    shim = here.parent / 'image_import'
    data = runpy.run_path(str(here.parent / 'door_placement/run.py'))['fixture']()
    with tempfile.TemporaryDirectory(prefix='geditor-weapon-placement-') as temp:
        work = Path(temp)
        (work / 'setup').mkdir()
        (work / 'setup/UsetupweaponZ.set').write_bytes(data)
        command = [os.environ.get('CC', 'cc'), '-O1', '-g', '-std=c99', '-Wall', '-Wextra', '-Werror',
                   '-ffunction-sections', '-fdata-sections', '-fsanitize=address,undefined',
                   f'-I{shim}', f'-I{src}', f'-I{root}', str(here / 'check.c'), str(shim / 'platform.c')]
        sources = ('setupload.c', 'actionblocks.c', 'bghistory.c', 'modelload.c', 'modelmaterials.c')
        subprocess.run(command + [str(src / name) for name in sources]
                       + ['-Wl,--gc-sections', '-lm', '-o', str(work / 'check')], check=True)
        subprocess.run([str(work / 'check'), str(work)], check=True,
                       env=dict(os.environ, ASAN_OPTIONS='detect_leaks=0', UBSAN_OPTIONS='halt_on_error=1'))


if __name__ == '__main__':
    main()
