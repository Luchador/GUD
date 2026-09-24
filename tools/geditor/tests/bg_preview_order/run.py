#!/usr/bin/env python3
"""Check edited BG draw order using the real mesh builder and viewport picker.

No ROM, Windows desktop or GL context is needed. An optional uncompressed
Control BG file also exercises the reported electric-warning decal geometry.
"""
import importlib.util
import os
from pathlib import Path
import re
import subprocess
import sys
import tempfile


def main():
    here = Path(__file__).resolve().parent
    src = here.parents[1] / 'src'
    sys.dont_write_bytecode = True
    spec = importlib.util.spec_from_file_location('extract', here.parent / 'vertex_eyedropper/run.py')
    helpers = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(helpers)
    viewport = (src / 'viewport.c').read_text()
    types = re.search(r'typedef enum ViewportRenderMode \{.*?\} ViewportRenderMode;',
                      (src / 'viewport.h').read_text(), re.S)[0] + '\n'
    for name in ('Vertex', 'SceneBatch', 'ViewportTexture', 'ViewportPickRay'):
        types += re.search(r'typedef struct ' + name + r' \{.*?\} ' + name + ';', viewport, re.S)[0] + '\n'
    for name in ('PICK_EPSILON', 'PICK_BARY_EPSILON', 'PICK_COPLANAR_EPSILON',
                 'PICK_COPLANAR_RELATIVE_EPSILON', 'BLEND_ALPHA_THRESHOLD', 'CUTOUT_ALPHA_THRESHOLD'):
        types += re.search(r'^#define VIEWPORT_' + name + r' .*', viewport, re.M)[0] + '\n'
    logic = ''.join(helpers.function(viewport, name) for name in (
        'ViewportBatchCullMode', 'ViewportTriangleHidden', 'ViewportBatchIsPickable',
        'ViewportTextureKey', 'ViewportCoplanarPickTolerance', 'ViewportRayTriangleDistance',
        'ViewportRayBatchTriangleGeometry', 'ViewportRayBatchTriangleDistance',
        'ViewportRaySelectableTriangleDistance', 'ViewportFindSceneTriangle'))
    with tempfile.TemporaryDirectory(prefix='geditor-bg-preview-order-') as temp:
        work = Path(temp)
        (work / 'types.inc').write_text(types)
        (work / 'logic.inc').write_text(logic)
        subprocess.run([os.environ.get('CC', 'cc'), '-O1', '-g', '-std=c99', '-Wall', '-Wextra',
                        '-Werror', '-Wno-unused-parameter', '-ffunction-sections', '-fdata-sections',
                        '-fsanitize=address,undefined', f'-I{here.parent / "image_import"}',
                        f'-I{src}', f'-I{work}', str(here / 'check.c'),
                        str(here.parent / 'image_import/platform.c')]
                       + [str(src / name) for name in
                          ('bgdocument.c', 'bgload.c', 'bgcompile.c', 'bgmaterial.c', 'bgrender.c')]
                       + ['-Wl,--gc-sections', '-lm', '-o', str(work / 'check')], check=True)
        subprocess.run([str(work / 'check'), *sys.argv[1:]], check=True,
                       env=dict(os.environ, ASAN_OPTIONS='detect_leaks=0', UBSAN_OPTIONS='halt_on_error=1'))


if __name__ == '__main__':
    main()
