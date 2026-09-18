#!/usr/bin/env python3
"""Knife geometry, native persistence, history, and failure atomicity checks."""
import importlib.util
import os
from pathlib import Path
import subprocess
import tempfile

here = Path(__file__).resolve().parent
src = here.parents[1] / 'src'
spec = importlib.util.spec_from_file_location('extract', here.parent / 'portal_editing/run.py')
extract = importlib.util.module_from_spec(spec)
spec.loader.exec_module(extract)
with tempfile.TemporaryDirectory(prefix='geditor-knife-') as temp:
    work = Path(temp)
    fixture = (here.parent / 'portals/check.c').read_text()
    (work / 'fixture.inc').write_text(''.join(extract.function(fixture, name) for name in ('Put', 'Float', 'Fixture')))
    (work / 'input.inc').write_text(extract.function((src / 'knife.c').read_text(), 'KnifeParseNumber'))
    subprocess.run([os.environ.get('CC', 'cc'), '-std=c99', '-O1', '-g', '-Wall', '-Wextra', '-Werror',
                    '-Wno-unused-parameter', '-ffunction-sections', '-fdata-sections', '-fsanitize=address,undefined',
                    f'-I{here.parent / "image_import"}', f'-I{src}', f'-I{work}',
                    str(here / 'check.c'), str(here.parent / 'image_import/platform.c')]
                   + [str(src / name) for name in ('bgknife.c', 'bgload.c', 'bgdocument.c', 'bgcompile.c', 'bgmaterial.c', 'bgrender.c', 'bghistory.c')]
                   + ['-Wl,--gc-sections', '-Wl,--wrap=malloc', '-Wl,--wrap=calloc', '-Wl,--wrap=realloc', '-lm', '-o', str(work / 'check')], check=True)
    subprocess.run([str(work / 'check'), str(work)], check=True,
                   env=dict(os.environ, ASAN_OPTIONS='detect_leaks=0', UBSAN_OPTIONS='halt_on_error=1'))

    import re
    viewport = (src / 'viewport.c').read_text()
    uv = (src / 'uvcanvas.c').read_text()
    def structure(text, name):
        return re.search(r'typedef struct ' + name + r' \{.*?\} ' + name + ';', text, re.S)[0] + '\n'
    (work / 'interaction_types.inc').write_text(''.join(structure(viewport, n) for n in
        ('Vertex', 'ViewportComponent', 'ViewportState', 'ViewportPickRay'))
        + ''.join(structure(uv, n) for n in ('UVCanvasNode', 'UVCanvasState')))
    names = ('ViewportUpdateGizmo', 'ViewportRefreshKnifePlane', 'ViewportSetKnifePlane',
             'ViewportKnifeActive', 'ViewportGetKnifePlane', 'ViewportTransformKnife',
             'ViewportShouldExtrudeEdges', 'ViewportPreviewEdgeExtrusion',
             'ViewportBeginTransform', 'ViewportBeginKnifeTransform', 'ViewportDragTransform', 'ViewportGetRotation', 'ViewportFinishKnifeTransform', 'ViewportCancelTransform', 'ViewportEndTransform')
    (work / 'plane.inc').write_text(re.search(r'^#define VIEWPORT_UNIFORM_SCALE_AXIS .*$', viewport, re.M)[0] + '\n' + ''.join(extract.function(viewport, n) for n in names))
    (work / 'uv_drag.inc').write_text(''.join(extract.function(uv, n) for n in
        ('UVCanvasTransformST', 'UVCanvasNodeST', 'UVCanvasTryTransform', 'UVCanvasDrag', 'UVCanvasCommit')))
    # Exercise the real one-point message routing, including Escape precedence.
    (work / 'pick.inc').write_text(extract.function((src / 'knife.c').read_text(), 'KnifeDialogHandleMessage'))
    binary = work / 'interaction'
    subprocess.run([os.environ.get('CC', 'cc'), '-std=c99', '-O1', '-g', '-Wall', '-Wextra', '-Werror',
                    '-Wno-unused-parameter', '-ffunction-sections', '-fdata-sections', '-fsanitize=address,undefined',
                    f'-I{here.parent / "image_import"}', f'-I{src}', f'-I{work}', str(here / 'interaction.c'),
                    str(src / 'rotation.c'), str(src / 'scaling.c'), str(src / 'bgknife.c'),
                    '-Wl,--gc-sections', '-lm', '-o', str(binary)], check=True)
    subprocess.run([str(binary)], check=True,
                   env=dict(os.environ, ASAN_OPTIONS='detect_leaks=0', UBSAN_OPTIONS='halt_on_error=1'))
