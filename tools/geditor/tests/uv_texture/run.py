#!/usr/bin/env python3
"""Shared-image selection, preview pixels/placement, and resizable UV controls."""
import importlib.util
import os
from pathlib import Path
import re
import subprocess
import sys
import tempfile

sys.dont_write_bytecode = True
HERE = Path(__file__).resolve().parent
SRC = HERE.parents[1] / 'src'
spec = importlib.util.spec_from_file_location('extract', HERE.parent / 'portal_editing/run.py')
extract = importlib.util.module_from_spec(spec)
spec.loader.exec_module(extract)
with tempfile.TemporaryDirectory(prefix='geditor-uv-texture-') as temp:
    work = Path(temp)
    canvas = (SRC / 'uvcanvas.c').read_text()
    editor = (SRC / 'uveditor.c').read_text()
    (work / 'types.inc').write_text(''.join(re.search(
        r'typedef struct ' + name + r' \{.*?\} ' + name + ';', canvas, re.S)[0] + '\n'
        for name in ('UVCanvasNode', 'UVCanvasState')))
    (work / 'canvas.inc').write_text(''.join(extract.function(canvas, name) for name in
        ('UVCanvasBlendTexture', 'UVCanvasSetTexture', 'UVCanvasSetTextureOpacity',
         'UVCanvasProject', 'UVCanvasClipCoordinate', 'UVCanvasClipEdge', 'UVCanvasDrawEdge',
         'UVCanvasDrawTexture', 'UVCanvasDrawLimits', 'UVCanvasDraw')))
    (work / 'editor.inc').write_text(''.join(extract.function(editor, name) for name in
        ('UVEditorIsOpen', 'UVEditorRefreshTexture', 'UVEditorRefreshSelection', 'UVEditorLayout')))
    subprocess.run([os.environ.get('CC', 'cc'), '-std=c99', '-O1', '-g', '-Wall', '-Wextra',
        '-Werror', '-Wno-unused-parameter', '-ffunction-sections', '-fdata-sections',
        '-fsanitize=address,undefined', f'-I{HERE.parent / "image_import"}', f'-I{SRC}', f'-I{work}',
        str(HERE / 'check.c'), str(SRC / 'bgdocument.c'), '-Wl,--gc-sections', '-lm',
        '-o', str(work / 'check')], check=True)
    subprocess.run([str(work / 'check')], check=True,
        env=dict(os.environ, ASAN_OPTIONS='detect_leaks=0', UBSAN_OPTIONS='halt_on_error=1'))
