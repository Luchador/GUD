#!/usr/bin/env python3
"""Exercise real picking, brush updates and viewport click/cancel dispatch.

Window-system calls are stubbed; ray/alpha/depth tests and source-color lookup
run unchanged. No ROM, OpenGL context or Windows desktop is required.
"""
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
    viewport = (src / 'viewport.c').read_text()
    picker = (src / 'colorpicker.c').read_text()
    editor = (src / 'geditor.c').read_text()
    types = re.search(r'typedef enum ViewportRenderMode \{.*?\} ViewportRenderMode;',
                      (src / 'viewport.h').read_text(), re.S)[0] + '\n'
    for name in ('Vertex', 'SceneBatch', 'ViewportTexture', 'ViewportPickRay'):
        types += re.search(r'typedef struct ' + name + r' \{.*?\} ' + name + ';', viewport, re.S)[0] + '\n'
    types += re.search(r'typedef struct ViewportBgVertexHit \{.*?\} ViewportBgVertexHit;',
                       (src / 'viewport.h').read_text(), re.S)[0] + '\n'
    for name in ('FOV_Y', 'NEAR_Z', 'FAR_Z', 'DEG_TO_RAD', 'PICK_EPSILON', 'PICK_BARY_EPSILON',
                 'PICK_COPLANAR_EPSILON', 'PICK_COPLANAR_RELATIVE_EPSILON',
                 'BLEND_ALPHA_THRESHOLD', 'CUTOUT_ALPHA_THRESHOLD'):
        types += re.search(r'^#define VIEWPORT_' + name + r' .*', viewport, re.M)[0] + '\n'
    types += '\n'.join(re.findall(r'^#define VIEWPORT_WM_.*', (src / 'viewport.h').read_text(), re.M)) + '\n'
    types += '#define COLORPICKER_IMAGE_SIZE 256\n'
    types += re.search(r'typedef struct ColorPickerState \{.*?\} ColorPickerState;', picker, re.S)[0] + '\n'
    logic = ''.join(function(picker, n) for n in ('ColorPickerRgbToHsv', 'ColorPickerSyncText',
        'ColorPickerSetColor', 'ColorPickerGetColor', 'ColorPickerSetSampling'))
    logic += ''.join(function(viewport, n) for n in ('ViewportGetTool', 'ViewportToolCursor',
        'ViewportRefreshCursor', 'ViewportSetColorPick'))
    logic += ''.join(function((src / 'bgdocument.c').read_text(), n) for n in
                     ('BgDocumentGetRoom', 'BgDocumentFindFace'))
    logic += ''.join(function(editor, n) for n in ('GEditorSampleBgVertex', 'GEditorSampleStanTile'))
    logic += ''.join(function((src / 'bgrender.c').read_text(), n) for n in
                     ('BgRenderWrapCoordinate', 'BgRenderWrapTexel'))
    logic += ''.join(function(viewport, n) for n in (
        'ViewportGetBasis', 'ViewportBatchCullMode', 'ViewportTriangleHidden', 'ViewportBatchIsPickable',
        'ViewportTextureKey', 'ViewportCoplanarPickTolerance', 'ViewportBuildPickRay',
        'ViewportRayTriangleDistance', 'ViewportRayBatchTriangleGeometry', 'ViewportRayBatchTriangleDistance',
        'ViewportRaySelectableTriangleDistance', 'ViewportFindSceneTriangle', 'ViewportFindVisibleSceneTriangle',
        'ViewportFindNearestBgTriangle', 'ViewportFindPaintTarget', 'ViewportPaintAt',
        'ViewportStanVisible', 'ViewportStanPointVertex', 'ViewportFindPickedStan',
        'ViewportSceneHitDistance', 'ViewportSampleColorAt'))
    wnd = function(viewport, 'ViewportWndProc')
    # Compile actual dispatch cases, retaining their early returns and fallthrough.
    dispatch = wnd[wnd.index('    case WM_LBUTTONDBLCLK:'):wnd.index('    case WM_LBUTTONUP:')]
    dispatch += wnd[wnd.index('    case WM_KEYDOWN:'):wnd.index('    case WM_MOUSEWHEEL:')]
    with tempfile.TemporaryDirectory(prefix='geditor-eyedropper-') as temp:
        work = Path(temp)
        (work / 'types.inc').write_text(types)
        (work / 'logic.inc').write_text(logic)
        (work / 'dispatch.inc').write_text(dispatch)
        subprocess.run([os.environ.get('CC', 'cc'), '-O1', '-g', '-std=c99', '-Wall', '-Wextra',
                        '-Werror', '-Wno-unused-parameter', '-fsanitize=address,undefined',
                        f'-I{here.parent / "image_import"}', f'-I{src}', f'-I{work}',
                        str(here / 'check.c'), '-lm', '-o', str(work / 'check')], check=True)
        subprocess.run([str(work / 'check')], check=True,
                       env=dict(os.environ, ASAN_OPTIONS='detect_leaks=0', UBSAN_OPTIONS='halt_on_error=1'))


if __name__ == '__main__':
    main()
