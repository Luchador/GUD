#!/usr/bin/env python3
"""Cylindrical fitting, native UV seams/history/save, and real coordinate controls."""
import importlib.util
import os
from pathlib import Path
import re
import subprocess
import sys
import tempfile


def main():
    sys.dont_write_bytecode = True
    here = Path(__file__).resolve().parent
    src = here.parents[1] / 'src'
    spec = importlib.util.spec_from_file_location('extract', here.parent / 'portal_editing/run.py')
    helper = importlib.util.module_from_spec(spec); spec.loader.exec_module(helper)
    with tempfile.TemporaryDirectory(prefix='geditor-uv-cylinder-') as temp:
        work = Path(temp)
        fixture = (here.parent / 'bg_transparency/edit.c').read_text()
        (work / 'fixture.inc').write_text(''.join(helper.function(fixture, n) for n in
            ('Put', 'Fixture', 'Refs', 'Equivalent', 'RoundTrip')))
        common = (here.parent / 'bg_disconnect/check.c').read_text()
        (work / 'common.inc').write_text(''.join(helper.function(common, n) for n in ('Same', 'UseCounts')))
        editor = (src / 'geditor.c').read_text()
        (work / 'editor.inc').write_text(helper.function(editor, 'GEditorApplyUVFaceEdit'))
        canvas = (src / 'uvcanvas.c').read_text()
        (work / 'types.inc').write_text(''.join(re.search(r'typedef struct ' + n + r' \{.*?\} ' + n + ';',canvas,re.S)[0]+'\n'
            for n in ('UVCanvasNode', 'UVCanvasState')))
        (work / 'canvas.inc').write_text(''.join(helper.function(canvas, n) for n in
            ('UVCanvasTransformST', 'UVCanvasNodeST', 'UVCanvasSelectionPosition', 'UVCanvasGetSelection',
             'UVCanvasSetCoordinate', 'UVCanvasGetTransform', 'UVCanvasHasFaces', 'UVCanvasProjectCylinder')))
        uv = (src / 'uveditor.c').read_text()
        (work / 'fields.inc').write_text(''.join(helper.function(uv, n) for n in
            ('UVEditorUpdateFields', 'UVEditorReadCoordinate', 'UVEditorApplyFields')))
        command = [os.environ.get('CC','cc'), '-std=c99', '-O1', '-g', '-Wall', '-Wextra', '-Werror',
                   '-Wno-unused-parameter', '-ffunction-sections', '-fdata-sections', '-fsanitize=address,undefined',
                   f'-I{here.parent / "image_import"}', f'-I{src}', f'-I{work}']
        env = dict(os.environ, ASAN_OPTIONS='detect_leaks=0', UBSAN_OPTIONS='halt_on_error=1')
        for name, sources in (('projection',('uvcylinder.c',)),
                              ('document',('bgload.c','bgcompile.c','bgmaterial.c','bgrender.c','bgprimitive.c','bghistory.c','uvcylinder.c')),
                              ('canvas',('uvcylinder.c',))):
            subprocess.run(command + [str(here / (name+'.c')), str(here.parent / 'image_import/platform.c')]
                + [str(src / n) for n in sources] + ['-Wl,--gc-sections', '-lm', '-o', str(work / name)],check=True)
            subprocess.run([str(work / name),str(work)],check=True,env=env)


if __name__ == '__main__':
    main()
