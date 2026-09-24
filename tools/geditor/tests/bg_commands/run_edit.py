#!/usr/bin/env python3
"""BG command document edits, native persistence and undo/redo under ASan/UBSan."""
import argparse
import os
from pathlib import Path
import re
import subprocess
import tempfile


def function(source, name):
    match = re.search(r'^[\w *]+\b' + name + r'\([^;{}]*\)\s*\{', source, re.M)
    start = source.index('{', match.start())
    depth, end = 1, start + 1
    while depth:
        depth += (source[end] == '{') - (source[end] == '}')
        end += 1
    return source[match.start():end] + '\n'


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('backgrounds', nargs='*', type=Path)
    args = parser.parse_args()
    here = Path(__file__).resolve().parent
    src = here.parents[1] / 'src'
    with tempfile.TemporaryDirectory(prefix='geditor-bg-command-edit-') as temp:
        work = Path(temp)
        fixture = (here.parent / 'portals/check.c').read_text()
        (work / 'fixture.inc').write_text(''.join(function(fixture, n) for n in ('Put', 'Get', 'Float', 'Fixture')))
        flags = [os.environ.get('CC', 'cc'), '-std=c99', '-O1', '-g', '-Wall', '-Wextra',
                 '-Werror', '-Wno-unused-parameter', '-Wno-format', '-ffunction-sections', '-fdata-sections',
                 '-fsanitize=address,undefined', f'-I{here.parent / "image_import"}', f'-I{src}', f'-I{work}']
        sources = ('bgcommands.c', 'bgload.c', 'bgdocument.c', 'bgcompile.c', 'bgportal.c', 'bgmaterial.c', 'bgrender.c', 'bghistory.c')
        subprocess.run(flags + [str(here / 'edit.c'), str(here.parent / 'image_import/platform.c')]
                       + [str(src / n) for n in sources]
                       + ['-Wl,--gc-sections', '-lm', '-o', str(work / 'check')], check=True)
        subprocess.run([str(work / 'check'), temp] + [str(p.resolve()) for p in args.backgrounds], check=True,
                       env=dict(os.environ, ASAN_OPTIONS=os.environ.get('ASAN_OPTIONS', 'detect_leaks=0'), UBSAN_OPTIONS='halt_on_error=1'))


if __name__ == '__main__':
    main()
