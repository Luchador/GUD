#!/usr/bin/env python3
"""Exercise production BG selection commands and their keyboard routing."""
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
    viewport = (src / 'viewport.c').read_text()
    types = ''
    for name in ('SceneBatch', 'Vertex', 'VertexColor', 'ViewportComponent',
                 'ViewportStanComponent', 'ViewportBoxPoint', 'ViewportBoxComponent'):
        types += re.search(r'typedef struct ' + name + r'\s*\{.*?\} ' + name + ';', viewport, re.S)[0] + '\n'
    logic = ''.join(function(viewport, name) for name in (
        'ViewportTriangleHidden', 'ViewportBatchIsPickable', 'ViewportSetTriangleColor',
        'ViewportClearBgSelection', 'ViewportClearAllSelection', 'ViewportCompareBoxPoints',
        'ViewportCompareBoxComponents', 'ViewportBoxComponentKey', 'ViewportApplyBoxComponents',
        'ViewportBgSelectionPoint', 'ViewportCanSelectBackground', 'ViewportSelectBackground',
        'ViewportGetSelectedBgTexture', 'ViewportCanSelectSameMaterial', 'ViewportSelectSameMaterial'))
    hotkeys = function((src / 'geditor.c').read_text(), 'GEditorHandleSelectionHotkey')
    with tempfile.TemporaryDirectory(prefix='geditor-bg-selection-') as temp:
        temp = Path(temp)
        (temp / 'types.inc').write_text(types)
        (temp / 'logic.inc').write_text(logic)
        (temp / 'hotkeys.inc').write_text(hotkeys)
        subprocess.run([os.environ.get('CC', 'cc'), '-std=c99', '-O1', '-g', '-Wall', '-Wextra',
                        '-Werror', '-Wno-unused-parameter', '-fsanitize=address,undefined',
                        f'-I{here.parent / "image_import"}', f'-I{src}', f'-I{temp}',
                        str(here / 'check.c'), '-lm', '-o', str(temp / 'check')], check=True)
        subprocess.run([str(temp / 'check')], check=True,
                       env=dict(os.environ, ASAN_OPTIONS='detect_leaks=0', UBSAN_OPTIONS='halt_on_error=1'))


if __name__ == '__main__':
    main()
