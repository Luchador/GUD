#!/usr/bin/env python3
"""Shared engine/editor disc geometry, actual runtime adapter and environment compatibility."""
import os
from pathlib import Path
import re
import subprocess
import tempfile

here = Path(__file__).resolve().parent
root = here.parents[3]

def function(source, name):
    match = re.search(r'^[\w *]+\b' + name + r'\([^;{}]*\)\s*\{', source, re.M)
    if not match:
        raise RuntimeError(name)
    start = source.index('{', match.start())
    depth, end = 1, start + 1
    while depth:
        depth += (source[end] == '{') - (source[end] == '}')
        end += 1
    return source[match.start():end] + '\n'

with tempfile.TemporaryDirectory(prefix='geditor-sky-body-') as folder:
    work = Path(folder)
    command = [os.environ.get('CC', 'cc'), '-std=c99', '-O1', '-g', '-Wall', '-Wextra', '-Werror',
               '-Wno-unused-parameter', '-fsanitize=address,undefined', f'-I{work}']
    env = dict(os.environ, ASAN_OPTIONS='detect_leaks=0', UBSAN_OPTIONS='halt_on_error=1')
    for name in ('math', 'runtime'):
        if name == 'runtime':
            source = (root/'src/game/sky.c').read_text()
            (work/'runtime.inc').write_text(function(source, 'skyRenderBody'))
            emitter = ''.join(function(source, fn) for fn in
                              ('skyClamp','skyRound','sub_GAME_7F094298','skyVerticesAreTheSame','skyRenderTri'))
            (work/'emitter.inc').write_text(emitter.replace('skyRenderTri(', 'RealSkyRenderTri('))
        subprocess.run(command + ['-Wno-unused-variable','-Wno-unused-but-set-variable'] + [str(here/f'{name}.c'), '-lm', '-o', str(work/name)], check=True)
        subprocess.run([str(work/name)], env=env, check=True)
