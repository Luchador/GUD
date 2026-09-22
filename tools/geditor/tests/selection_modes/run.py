#!/usr/bin/env python3
"""Exercise production mode switching with BG, STAN and portal selections."""
import os
from pathlib import Path
import re
import subprocess
import tempfile


def function(source, name):
    match = re.search(r'^(?:static )?(?:unsigned )?\w+ ' + name + r'\([^;{}]*\)\s*\{', source, re.M)
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
    types = re.search(r'^#define VIEWPORT_SELECTION_GOLD .*', viewport, re.M)[0] + '\n'
    for name in ('SceneBatch', 'Vertex', 'VertexColor', 'ViewportComponent',
                 'ViewportStanComponent', 'ViewportBoxPoint', 'ViewportBoxComponent',
                 'ViewportModeSelection'):
        types += re.search(r'typedef struct ' + name + r'\s*\{.*?\} ' + name + ';', viewport, re.S)[0] + '\n'
    logic = ''.join(function(viewport, name) for name in (
        'ViewportTriangleHidden', 'ViewportBatchIsPickable',
        'ViewportSetFullbrightColor', 'ViewportSetTriangleColor',
        'ViewportClearBgSelection', 'ViewportClearStanSelection', 'ViewportClearAllSelection',
        'ViewportStanVisible', 'ViewportCompareStanIds', 'ViewportStanTileHidden', 'ViewportStanPointRef',
        'ViewportCompareBoxPoints', 'ViewportCompareBoxComponents', 'ViewportBoxComponentKey',
        'ViewportBgSelectionPoint', 'ViewportPortalComponentMask', 'ViewportPortalPointMask',
        'ViewportPortalGeometryIsFirst', 'ViewportResolveActivePortal',
        'ViewportConvertPolygonSelection', 'ViewportCollectModeSelection',
        'ViewportTransferSelection', 'ViewportSetTool'))
    with tempfile.TemporaryDirectory(prefix='geditor-selection-modes-') as temp:
        temp = Path(temp)
        (temp / 'types.inc').write_text(types)
        (temp / 'logic.inc').write_text(logic)
        subprocess.run([os.environ.get('CC', 'cc'), '-std=c99', '-O1', '-g', '-Wall', '-Wextra',
                        '-Werror', '-Wno-unused-parameter', '-fsanitize=address,undefined',
                        f'-I{here.parent / "image_import"}', f'-I{src}', f'-I{temp}',
                        str(here / 'check.c'), '-lm', '-o', str(temp / 'check')], check=True)
        subprocess.run([str(temp / 'check')], check=True,
                       env=dict(os.environ, ASAN_OPTIONS='detect_leaks=0', UBSAN_OPTIONS='halt_on_error=1'))


if __name__ == '__main__':
    main()
