#!/usr/bin/env python3
"""Exercise production display colors, wire overlays, alpha picking and hotkeys.

Only GL calls, window messages and unrelated environment/extrusion helpers are
stubbed. No ROM or Windows desktop is needed.
"""
import os
from pathlib import Path
import re
import subprocess
import tempfile


def function(source, name):
    match = re.search(r'^[\w ]+ ' + name + r'\([^;{}]*\)\s*\{', source, re.M)
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
    editor = (src / 'geditor.c').read_text()
    types = re.search(r'typedef enum ViewportRenderMode \{.*?\} ViewportRenderMode;',
                      (src / 'viewport.h').read_text(), re.S)[0] + '\n'
    for name in ('Vertex', 'VertexColor', 'SceneBatch', 'ViewportTexture', 'ViewportPickRay'):
        types += re.search(r'typedef struct ' + name + r' \{.*?\} ' + name + ';', viewport, re.S)[0] + '\n'
    for name in ('SELECTION_GOLD', 'VERTEX_MARKER_SIZE', 'PICK_EPSILON', 'PICK_BARY_EPSILON',
                 'BLEND_ALPHA_THRESHOLD', 'CUTOUT_ALPHA_THRESHOLD',
                 'PICK_COPLANAR_RELATIVE_EPSILON', 'PICK_COPLANAR_EPSILON'):
        types += re.search(r'^#define VIEWPORT_' + name + r' .*', viewport, re.M)[0] + '\n'
    logic = ''.join(function((src / 'bgrender.c').read_text(), name) for name in (
        'BgRenderWrapCoordinate', 'BgRenderWrapTexel'))
    logic += ''.join(function(viewport, name) for name in (
        'ViewportBatchCullMode', 'ViewportApplyCullMode', 'ViewportTriangleHidden',
        'ViewportTextureKey', 'ViewportSetFullbrightColor', 'ViewportSceneColorPointer',
        'ViewportSetTriangleColor', 'ViewportGetRenderMode', 'ViewportSetRenderMode',
        'ViewportDrawVisibleBatch', 'ViewportDrawBgToolOverlay',
        'ViewportRayTriangleDistance', 'ViewportRayBatchTriangleGeometry',
        'ViewportRayBatchTriangleDistance', 'ViewportRaySelectableTriangleDistance',
        'ViewportBatchIsPickable', 'ViewportCoplanarPickTolerance', 'ViewportFindSceneTriangle',
        'ViewportFindVisibleSceneTriangle', 'ViewportFindPickedTriangle'))
    with tempfile.TemporaryDirectory(prefix='geditor-view-modes-') as temp:
        temp = Path(temp)
        (temp / 'types.inc').write_text(types)
        (temp / 'logic.inc').write_text(logic)
        (temp / 'hotkeys.inc').write_text(function(editor, 'GEditorHandleRenderModeHotkey'))
        subprocess.run([os.environ.get('CC', 'cc'), '-std=c99', '-O1', '-g', '-Wall', '-Wextra',
                        '-Werror', '-Wno-unused-parameter', '-fsanitize=address,undefined',
                        f'-I{here.parent / "image_import"}', f'-I{src}', f'-I{temp}',
                        str(here / 'check.c'), '-lm', '-o', str(temp / 'check')], check=True)
        subprocess.run([str(temp / 'check')], check=True,
                       env=dict(os.environ, ASAN_OPTIONS='detect_leaks=0', UBSAN_OPTIONS='halt_on_error=1'))


if __name__ == '__main__':
    main()
