#!/usr/bin/env python3
"""Check production vertex visibility and snap gestures without a Windows UI."""
import argparse
import os
from pathlib import Path
import re
import subprocess
import tempfile


def function(source, name):
    match = re.search(r'^(?:static )?\w+ ' + name + r'\([^;{}]*\)\s*\{', source, re.M)
    if match is None:
        raise RuntimeError(f'Missing function: {name}')
    start = source.index('{', match.start())
    depth, end = 1, start + 1
    while depth:
        depth += (source[end] == '{') - (source[end] == '}')
        end += 1
    return source[match.start():end] + '\n'


def main():
    here = Path(__file__).resolve().parent
    src = here.parents[1] / 'src'
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--viewport', type=Path, default=src / 'viewport.c')
    args = parser.parse_args()
    source = args.viewport.read_text()
    types = ''.join(re.search(r'typedef struct ' + name + r'\s*\{.*?\} ' + name + ';',
                             source, re.S)[0] + '\n' for name in
                    ('Vertex', 'SceneBatch', 'ViewportPickRay', 'ViewportComponent', 'ViewportStanComponent'))
    types += '\n'.join(line for line in source.splitlines() if line.startswith('#define VIEWPORT_PICK_'))
    with tempfile.TemporaryDirectory(prefix='geditor-vertex-snap-') as temp:
        work = Path(temp)
        (work / 'types.inc').write_text(types + '\n')
        (work / 'rays.inc').write_text(function(source, 'ViewportRayTriangleDistance') +
                                      function(source, 'ViewportCoplanarPickTolerance'))
        (work / 'visible.inc').write_text(function(source, 'ViewportComponentVisible'))
        (work / 'gesture.inc').write_text(function(source, 'ViewportSnapVertexAt'))
        for case in ('visibility', 'gesture'):
            subprocess.run([os.environ.get('CC', 'cc'), '-std=c99', '-O1', '-g', '-Wall', '-Wextra',
                            '-Werror', '-Wno-unused-parameter', '-fsanitize=address,undefined',
                            f'-I{here.parent / "image_import"}', f'-I{src}', f'-I{work}',
                            str(here / (case + '.c')), '-lm', '-o', str(work / case)], check=True)
            subprocess.run([str(work / case)], check=True,
                           env=dict(os.environ, ASAN_OPTIONS='detect_leaks=0', UBSAN_OPTIONS='halt_on_error=1'))


if __name__ == '__main__':
    main()
