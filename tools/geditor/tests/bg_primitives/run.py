#!/usr/bin/env python3
"""Primitive authoring, native BG save/compile, undo, and viewport drops."""
import os
from pathlib import Path
import re
import subprocess
import tempfile


def function(source, name):
    match = re.search(r'^(?:static )?\w+ ' + name + r'\([^;{}]*\)\s*\{', source, re.M)
    if match is None:
        raise RuntimeError(f'Missing production function: {name}')
    start = source.index('{', match.start())
    depth, end = 1, start + 1
    while depth:
        depth += (source[end] == '{') - (source[end] == '}')
        end += 1
    return source[match.start():end] + '\n'


def main():
    here = Path(__file__).resolve().parent
    src = here.parents[1] / 'src'
    shim = here.parent / 'image_import'
    with tempfile.TemporaryDirectory(prefix='geditor-primitives-') as temp:
        work = Path(temp)
        command = [os.environ.get('CC', 'cc'), '-O1', '-g', '-std=c99', '-Wall', '-Wextra',
                   '-Wno-unused-parameter', '-ffunction-sections', '-fdata-sections',
                   '-fsanitize=address,undefined', f'-I{shim}', f'-I{src}', f'-I{work}']
        sources = ['bgload.c', 'bgdocument.c', 'bgcompile.c', 'bgmaterial.c', 'bgrender.c', 'bghistory.c']
        subprocess.run(command + [str(here / 'check.c'), str(shim / 'platform.c')]
                       + [str(src / name) for name in sources]
                       + ['-Wl,--gc-sections', '-lm', '-o', str(work / 'check')], check=True)
        env = dict(os.environ, ASAN_OPTIONS='detect_leaks=0', UBSAN_OPTIONS='halt_on_error=1')
        subprocess.run([str(work / 'check'), str(work)], check=True, env=env)
        subprocess.run(command + [str(here / 'options.c'), '-lm', '-o', str(work / 'options')], check=True)
        subprocess.run([str(work / 'options')], check=True, env=env)
        viewport = (src / 'viewport.c').read_text()
        logic = '\n'.join(line for line in viewport.splitlines()
                          if line.startswith(('#define VIEWPORT_FOV_Y ', '#define VIEWPORT_NEAR_Z ',
                                              '#define VIEWPORT_FAR_Z ', '#define VIEWPORT_DEG_TO_RAD '))) + '\n'
        logic += re.search(r'typedef struct ViewportPickRay \{.*?\} ViewportPickRay;', viewport, re.S)[0] + '\n'
        logic += ''.join(function(viewport, name) for name in ('ViewportGetBasis', 'ViewportBuildPickRay'))
        (work / 'ray.inc').write_text(logic)
        (work / 'drop.inc').write_text(function(viewport, 'ViewportGetPrimitiveDrop'))
        (work / 'select.inc').write_text(''.join(function(viewport, name) for name in
            ('ViewportCompareFaceRefs', 'ViewportTriangleHidden', 'ViewportSelectBgFaces')))
        subprocess.run(command + [str(here / 'drop.c'), '-lm', '-o', str(work / 'drop')], check=True)
        subprocess.run([str(work / 'drop')], check=True, env=env)


if __name__ == '__main__':
    main()
