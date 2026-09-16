#!/usr/bin/env python3
"""Drive UV drags through the production history-menu/inspector refresh path."""
import importlib.util
import os
from pathlib import Path
import re
import subprocess
import sys
import tempfile

sys.dont_write_bytecode = True
here = Path(__file__).resolve().parent
src = here.parents[1] / 'src'
spec = importlib.util.spec_from_file_location('extract', here.parent / 'portal_editing/run.py')
helper = importlib.util.module_from_spec(spec)
spec.loader.exec_module(helper)
canvas = (src / 'uvcanvas.c').read_text()
editor = (src / 'geditor.c').read_text()
with tempfile.TemporaryDirectory(prefix='geditor-uv-interaction-') as temp:
    work = Path(temp)
    (work / 'types.inc').write_text(''.join(re.search(
        r'typedef struct ' + name + r' \{.*?\} ' + name + ';', canvas, re.S)[0] + '\n'
        for name in ('UVCanvasNode', 'UVCanvasState')))
    (work / 'canvas.inc').write_text(''.join(helper.function(canvas, name) for name in
        ('UVCanvasNodeCompare', 'UVCanvasCornerCompare', 'UVCanvasResetTransform',
         'UVCanvasTransformST', 'UVCanvasNodeST', 'UVCanvasSelectionPosition',
         'UVCanvasProject', 'UVCanvasGizmo', 'UVCanvasPickHandle', 'UVCanvasNotify',
         'UVCanvasUpdatePreview', 'UVCanvasCancelInteraction', 'UVCanvasTryTransform',
         'UVCanvasDrag', 'UVCanvasCommit', 'UVCanvasSetTriangles', 'UVCanvasSetTransformMode',
         'UVCanvasApplyTransform')))
    # Also runs against the pre-fix source, where the inspector was not split.
    names = ['GEditorRefreshSelectionDetails', 'GEditorRefreshHistoryMenu']
    if 'static void GEditorRefreshSelectionInspector(' in editor:
        names.insert(0, 'GEditorRefreshSelectionInspector')
    (work / 'editor.inc').write_text(''.join(helper.function(editor, name) for name in names))
    command = [os.environ.get('CC', 'cc'), '-std=c99', '-O1', '-g', '-Wall', '-Wextra',
        '-Werror', '-Wno-unused-parameter', '-fsanitize=address,undefined',
        f'-I{here.parent / "image_import"}', f'-I{src}', f'-I{work}',
        str(here / 'check.c'), str(src / 'rotation.c'), '-lm', '-o', str(work / 'check')]
    subprocess.run(command, check=True)
    subprocess.run([str(work / 'check')], check=True,
        env=dict(os.environ, ASAN_OPTIONS='detect_leaks=0', UBSAN_OPTIONS='halt_on_error=1'))
