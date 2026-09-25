#!/usr/bin/env python3
"""Native vertex seams, editor transactions and viewport context gestures."""
import importlib.util
import os
import re
from pathlib import Path
import subprocess
import sys
import tempfile


def main():
    here = Path(__file__).resolve().parent
    src = here.parents[1] / 'src'
    shim = here.parent / 'image_import'
    sys.dont_write_bytecode = True
    spec = importlib.util.spec_from_file_location('extract', here.parent / 'view_modes/run.py')
    helpers = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(helpers)
    with tempfile.TemporaryDirectory(prefix='geditor-disconnect-') as temp:
        work = Path(temp)
        fixture = (here.parent / 'bg_transparency/edit.c').read_text()
        (work / 'fixture.inc').write_text(''.join(helpers.function(fixture, name) for name in
            ('Put', 'Fixture', 'Refs', 'Equivalent')))
        (work / 'editor.inc').write_text(helpers.function((src / 'geditor.c').read_text(), 'GEditorSeparateBgVertices'))
        command = [os.environ.get('CC', 'cc'), '-O1', '-g', '-std=c99', '-Wall', '-Wextra',
                   '-Werror', '-Wno-unused-parameter', '-ffunction-sections', '-fdata-sections',
                   '-fsanitize=address,undefined', f'-I{shim}', f'-I{src}', f'-I{work}']
        sources = ('bgload.c', 'bgdocument.c', 'bgcompile.c', 'bgmaterial.c', 'bgrender.c', 'bghistory.c', 'bguv.c')
        subprocess.run(command + [str(here / 'check.c'), str(shim / 'platform.c')]
                       + [str(src / name) for name in sources]
                       + ['-Wl,--gc-sections', '-lm', '-o', str(work / 'check')], check=True)
        env = dict(os.environ, ASAN_OPTIONS='detect_leaks=0', UBSAN_OPTIONS='halt_on_error=1')
        subprocess.run([str(work / 'check'), str(work)], check=True, env=env)

        viewport = (src / 'viewport.c').read_text()
        types = re.search(r'typedef enum ViewportRenderMode \{.*?\} ViewportRenderMode;',
                          (src / 'viewport.h').read_text(), re.S)[0] + '\n'
        for name in ('Vertex', 'SceneBatch', 'ViewportTexture', 'ViewportPickRay'):
            types += re.search(r'typedef struct ' + name + r' \{.*?\} ' + name + ';', viewport, re.S)[0] + '\n'
        types += re.search(r'typedef struct ViewportObjectPaste \{.*?\} ViewportObjectPaste;',
                           (src / 'viewport.h').read_text(), re.S)[0] + '\n'
        for name in ('FOV_Y', 'NEAR_Z', 'FAR_Z', 'DEG_TO_RAD', 'LOOK_SENSITIVITY', 'PITCH_LIMIT',
                     'PICK_EPSILON', 'PICK_BARY_EPSILON', 'PICK_COPLANAR_EPSILON',
                     'PICK_COPLANAR_RELATIVE_EPSILON', 'BLEND_ALPHA_THRESHOLD', 'CUTOUT_ALPHA_THRESHOLD'):
            types += re.search(r'^#define VIEWPORT_' + name + r' .*', viewport, re.M)[0] + '\n'
        (work / 'types.inc').write_text(types)
        logic = ''.join(helpers.function((src / 'bgrender.c').read_text(), name) for name in
                        ('BgRenderWrapCoordinate', 'BgRenderWrapTexel'))
        names = ('ViewportBatchCullMode', 'ViewportTriangleHidden', 'ViewportBatchIsPickable',
                 'ViewportTextureKey', 'ViewportCompareVertexRefs', 'ViewportCoplanarPickTolerance',
                 'ViewportBuildPickRay', 'ViewportRayTriangleDistance', 'ViewportRayBatchTriangleGeometry',
                 'ViewportRayBatchTriangleDistance', 'ViewportRaySelectableTriangleDistance',
                 'ViewportFindSceneTriangle', 'ViewportFindPickedTriangle',
                 'ViewportFindVisibleSceneTriangle', 'ViewportFindNearestBgTriangle', 'ViewportObjectPasteTarget',
                 'ViewportProject', 'ViewportProjectEdgePoint', 'ViewportComponentVisible',
                 'ViewportFindContextEdge', 'ViewportShowGeometryContextMenu',
                 'ViewportBeginFly', 'ViewportEndFly', 'ViewportFlyLook', 'ViewportFlyFrame',
                 'ViewportBeginRightGesture', 'ViewportEndRightGesture')
        # No setup glass in this BG fixture.
        logic = 'static int ViewportGlassAlpha(const ViewportState *s, const SceneBatch *b) { return 0; }\n' + logic
        (work / 'input.inc').write_text(logic + ''.join(helpers.function(viewport, name) for name in names))
        subprocess.run(command + [str(here / 'input.c'), '-Wl,--gc-sections', '-lm',
                                  '-o', str(work / 'input')], check=True)
        subprocess.run([str(work / 'input')], check=True, env=env)



if __name__ == '__main__':
    main()
