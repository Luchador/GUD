#!/usr/bin/env python3
"""Compare all native tile colors and exercise real placed models without a ROM/UI."""
import os
from pathlib import Path
import subprocess
import tempfile


def main():
    here = Path(__file__).resolve().parent
    src = here.parents[1] / 'src'
    root = src.parents[2]
    shim = here.parent / 'image_import'
    game = (root / 'src/game/propobj.c').read_text()
    start = game.index('void objSetColorFromTile(')
    end = game.index('\n\nvoid objSetShading', start)
    reference = game[start:end]
    with tempfile.TemporaryDirectory(prefix='geditor-object-shading-') as work:
        work = Path(work)
        (work / 'reference.h').write_text(reference)
        command = [os.environ.get('CC', 'cc'), '-std=c99', '-O1', '-g', '-Wall', '-Wextra',
                   '-Wno-unused-parameter', '-ffunction-sections', '-fdata-sections',
                   '-fsanitize=address,undefined', f'-I{shim}', f'-I{src}', f'-I{root}', f'-I{work}',
                   str(here / 'check.c'), str(shim / 'platform.c')]
        command += [str(src / n) for n in ('objectshade.c', 'modelload.c', 'bgmaterial.c',
                    'bgrender.c', 'setupload.c', 'stanquery.c')]
        command += ['-Wl,--gc-sections', '-lm', '-o', str(work / 'check')]
        subprocess.run(command, check=True)
        subprocess.run([str(work / 'check'), str(root)], check=True,
                       env=dict(os.environ, ASAN_OPTIONS='detect_leaks=0', UBSAN_OPTIONS='halt_on_error=1'))


if __name__ == '__main__':
    main()
