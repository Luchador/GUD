#!/usr/bin/env python3
"""Safe pairs, native item links, real model placement and game's pickup gate."""
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
    fixture = runpy.run_path(str(here.parent / 'door_placement/run.py'))['fixture']()
    extract = runpy.run_path(str(here.parent / 'object_properties/run.py'))['function']
    with tempfile.TemporaryDirectory(prefix='geditor-safe-') as temp:
        work = Path(temp)
        (work / 'setup').mkdir()
        (work / 'setup/UsetupsafeZ.set').write_bytes(fixture)
        (work / 'pickup.inc').write_text(extract((root / 'src/game/chrprop.c').read_text(), 'objCanPickupFromSafe'))
        command = [os.environ.get('CC', 'cc'), '-O1', '-g', '-std=c99', '-Wall', '-Wextra',
                   '-Wno-unused-parameter', '-Wno-unused-variable', '-ffunction-sections', '-fdata-sections',
                   '-fsanitize=address,undefined', f'-I{shim}', f'-I{src}', f'-I{root}', f'-I{work}']
        sources = ('setupload.c', 'actionblocks.c', 'bghistory.c', 'objectload.c', 'objectshade.c',
                   'modelload.c', 'modelmaterials.c', 'bgmaterial.c', 'bgrender.c', 'stanquery.c',
                   'rotation.c', 'scaling.c', 'doorshadow.c', '../../../src/game/doorshadowmath.c')
        subprocess.run(command + [str(here / 'check.c'), str(here / 'assets.c'), str(shim / 'platform.c')]
                       + [str(src / name) for name in sources]
                       + ['-Wl,--gc-sections', '-lm', '-o', str(work / 'check')], check=True)
        subprocess.run([str(work / 'check'), str(work), str(root)], check=True,
                       env=dict(os.environ, ASAN_OPTIONS='detect_leaks=0', UBSAN_OPTIONS='halt_on_error=1'))


if __name__ == '__main__':
    main()
