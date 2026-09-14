#!/usr/bin/env python3
"""Native portal edits and actual viewport selection, picking, preview and controller."""
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
    with tempfile.TemporaryDirectory(prefix='geditor-portal-editing-') as temp:
        work = Path(temp)
        fixture = (here.parent / 'portals/check.c').read_text()
        (work / 'fixture.inc').write_text(''.join(function(fixture, n) for n in ('Put', 'Get', 'Float', 'Fixture')))
        types = ''.join(re.search(r'typedef struct ' + n + r' \{.*?\} ' + n + ';', viewport, re.S)[0] + '\n'
                        for n in ('Vertex', 'ViewportPickRay'))
        types += '\n'.join(l for l in viewport.splitlines() if l.startswith(('#define VIEWPORT_PICK_', '#define VIEWPORT_PORTAL_', '#define VIEWPORT_SELECTION_GOLD')))
        for name in ('FOV_Y', 'NEAR_Z', 'FAR_Z', 'DEG_TO_RAD'):
            types += '\n' + re.search(r'^#define VIEWPORT_' + name + r' .*', viewport, re.M)[0]
        (work / 'types.inc').write_text(types + '\n')
        names = ('ViewportGetBasis', 'ViewportProject', 'ViewportBuildPickRay', 'ViewportRayTriangleDistance',
                 'ViewportCoplanarPickTolerance', 'ViewportProjectEdgePoint', 'ViewportPortalGeometryIsFirst',
                 'ViewportPortalComponentMask', 'ViewportPortalPointMask', 'ViewportPortalSelectionPosition',
                 'ViewportGetPortalSelectionCount', 'ViewportGetMovePortalPoints', 'ViewportFindPickedPortal',
                 'ViewportPortalPointVisible', 'ViewportFindPortalComponent', 'ViewportResolveActivePortal',
                 'ViewportSetPortalVertex', 'ViewportRefreshPortalColors', 'ViewportRefreshPortalGeometry',
                 'ViewportTryPickPortal', 'ViewportVertexInBox', 'ViewportEdgeInBox', 'ViewportApplyPortalBox',
                 'ViewportPreparePortalDrag', 'ViewportPreviewPortalDrag', 'ViewportSetPortals')
        (work / 'viewport.inc').write_text(''.join(function(viewport, n) for n in names))
        (work / 'controller.inc').write_text(function((src/'geditor.c').read_text(), 'GEditorTranslatePortals'))
        command = [os.environ.get('CC', 'cc'), '-std=c99', '-O1', '-g', '-Wall', '-Wextra', '-Werror',
                   '-Wno-unused-parameter', '-ffunction-sections', '-fdata-sections', '-fsanitize=address,undefined',
                   f'-I{here.parent / "image_import"}', f'-I{src}', f'-I{work}']
        sources = ('bgload.c', 'bgdocument.c', 'bgcompile.c', 'bgmaterial.c', 'bgrender.c', 'bghistory.c')
        subprocess.run(command + [str(here/'check.c'), str(here.parent/'image_import/platform.c')]
                       + [str(src/n) for n in sources] + ['-Wl,--gc-sections', '-lm', '-o', str(work/'check')], check=True)
        subprocess.run([str(work/'check')], check=True,
                       env=dict(os.environ, ASAN_OPTIONS='detect_leaks=0', UBSAN_OPTIONS='halt_on_error=1'))


if __name__ == '__main__':
    main()
