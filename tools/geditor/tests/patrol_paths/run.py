#!/usr/bin/env python3
"""Check patrol decoding under ASan/UBSan. Optional argument: project directory.

With a project directory, also compare every setup's preview connections to an
independent simulation of the game's forward/reverse patrol traversal.
"""
import os
import re
from pathlib import Path
import struct
import subprocess
import sys
import tempfile


def function(source, name):
    match = re.search(r'^static [^\n]+\b' + name + r'\([^;{}]*\)\s*\{', source, re.M)
    assert match, name
    end = source.index('{', match.start()) + 1
    depth = 1
    while depth:
        depth += (source[end] == '{') - (source[end] == '}')
        end += 1
    return source[match.start():end] + '\n'


def expected(data):
    word = lambda offset: struct.unpack_from('>i', data, offset)[0]
    waypoints, paths = word(0), word(16)
    edges = {}
    while paths and word(paths):
        at = word(paths)
        route = []
        while word(at) >= 0:
            route.append(word(waypoints + word(at) * 16))
            at += 4
        if route:
            travel = route + ([route[0]] if data[paths + 5] & 1 else route[-2::-1])
            for start, end in zip(travel, travel[1:]):
                if start != end:
                    pair = tuple(sorted((start, end)))
                    edges[pair] = edges.get(pair, 0) | (1 if start < end else 2)
        paths += 8
    return [(a, b, flags) for (a, b), flags in sorted(edges.items())]


def main():
    here = Path(__file__).resolve().parent
    src = here.parents[1] / 'src'
    shim = here.parent / 'image_import'
    project = Path(sys.argv[1]).resolve() if len(sys.argv) > 1 else None
    setups = sorted((project / 'setup').glob('*.set')) if project else []
    with tempfile.TemporaryDirectory(prefix='geditor-patrol-paths-') as folder:
        binary = Path(folder) / 'check'
        viewport = (src / 'viewport.c').read_text()
        code = '\n'.join(re.search(r'^#define VIEWPORT_' + name + r' .*', viewport, re.M)[0]
                         for name in ('FOV_Y', 'DEG_TO_RAD', 'NEAR_Z', 'PATH_COLOR')) + '\n'
        code += ''.join(function(viewport, name) for name in ('ViewportGetBasis',
            'ViewportGroundPadPosition', 'ViewportPadPosition', 'ViewportRefreshPadPreview',
            'ViewportSelectedPadIndex', 'ViewportPadVisible', 'ViewportRefreshPadColors',
            'ViewportPatrolEndpoints', 'ViewportDrawPatrolPaths'))
        (Path(folder) / 'patrol_viewport.h').write_text(code)
        command = [os.environ.get('CC', 'cc'), '-O1', '-g', '-std=c99', '-Wall', '-Wextra',
                   '-Wno-unused-parameter', '-ffunction-sections', '-fdata-sections',
                   '-fsanitize=address,undefined', f'-I{shim}', f'-I{src}', f'-I{src.parents[2]}',
                   f'-I{folder}', str(here / 'check.c'), str(here / 'viewport.c'),
                   str(shim / 'platform.c'), str(src / 'setupload.c'),
                   str(src / 'rotation.c'), str(src / 'scaling.c'), str(src / 'stanquery.c'),
                   '-Wl,--gc-sections', '-lm', '-o', str(binary)]
        subprocess.run(command, check=True)
        args = [str(binary)] + ([str(project)] + [s.stem for s in setups] if project else [])
        result = subprocess.run(args, capture_output=True, text=True,
                                env=dict(os.environ, ASAN_OPTIONS=os.environ.get('ASAN_OPTIONS', 'detect_leaks=0'), UBSAN_OPTIONS='halt_on_error=1'))
        if result.returncode:
            print(result.stdout, end='')
            print(result.stderr, end='', file=sys.stderr)
            result.check_returncode()
        actual = {}
        for line in result.stdout.splitlines():
            if line.startswith('LINKS '):
                _, name, *links = line.split()
                actual[name] = [tuple(map(int, link.split(','))) for link in links]
            else:
                print(line)
        for setup in setups:
            assert actual[setup.stem] == expected(setup.read_bytes()), setup.name
        if setups:
            print(f'PASS: {len(setups)} stock/project setups, {sum(map(len, actual.values()))} unique patrol connections match runtime traversal.')


if __name__ == '__main__':
    main()
