#!/usr/bin/env python3
"""New portal geometry, native persistence, history and drop-controller tests."""
import os
from pathlib import Path
import re
import subprocess
import tempfile


def function(source, name):
    match = re.search(r'^[\w *]+\b' + name + r'\([^;{}]*\)\s*\{', source, re.M)
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
    with tempfile.TemporaryDirectory(prefix='geditor-add-portal-') as temp:
        work = Path(temp)
        fixture = (here.parent / 'portals/check.c').read_text()
        (work / 'fixture.inc').write_text(''.join(function(fixture, n) for n in ('Put', 'Get', 'Float', 'Fixture')))
        editor = (src / 'geditor.c').read_text()
        (work / 'editor.inc').write_text(''.join(function(editor, n) for n in ('GEditorCanAddPortal', 'GEditorDropPortal')))
        command = [os.environ.get('CC', 'cc'), '-O1', '-g', '-std=c99', '-Wall', '-Wextra',
                   '-Werror', '-Wno-unused-parameter', '-ffunction-sections', '-fdata-sections',
                   '-fsanitize=address,undefined', f'-I{here.parent / "image_import"}', f'-I{src}', f'-I{work}']
        sources = ('bgload.c', 'bgdocument.c', 'bgcompile.c', 'bgmaterial.c', 'bgrender.c', 'bghistory.c')
        subprocess.run(command + [str(here / 'check.c'), str(here.parent / 'image_import/platform.c')]
                       + [str(src / n) for n in sources]
                       + ['-Wl,--gc-sections', '-lm', '-o', str(work / 'check')], check=True)
        subprocess.run([str(work / 'check'), str(work)], check=True,
                       env=dict(os.environ, ASAN_OPTIONS='detect_leaks=0', UBSAN_OPTIONS='halt_on_error=1'))


if __name__ == '__main__':
    main()
