#!/usr/bin/env python3
"""Reverse Edge topology, native persistence, history and T routing.
"""
import importlib.util
import os
from pathlib import Path
import subprocess
import tempfile

here = Path(__file__).resolve().parent
src = here.parents[1] / 'src'
spec = importlib.util.spec_from_file_location('extract', here.parent / 'bg_primitives/run.py')
helpers = importlib.util.module_from_spec(spec)
spec.loader.exec_module(helpers)
with tempfile.TemporaryDirectory(prefix='geditor-reverse-') as temp:
    work = Path(temp)
    fixture = (here.parent / 'bg_primitives/check.c').read_text()
    (work / 'fixture.inc').write_text(''.join(helpers.function(fixture, n) for n in
        ('Put', 'Float', 'Fixture', 'RoundTrip')))
    source = (here.parent / 'geometry_toolbar/bridge.c').read_text()
    (work / 'counts.inc').write_text(helpers.function(source, 'Counts'))
    editor = (src / 'geditor.c').read_text()
    (work / 'editor.inc').write_text(''.join(helpers.function(editor, n) for n in
        ('GEditorCanReverseSelectedEdge', 'GEditorReverseSelectedEdge', 'GEditorHandleReverseEdgeHotkey')))
    cmd = [os.environ.get('CC', 'cc'), '-O1', '-g', '-std=c99', '-Wall', '-Wextra', '-Werror',
           '-Wno-unused-parameter', '-ffunction-sections', '-fdata-sections', '-fsanitize=address,undefined',
           f'-I{here.parent / "image_import"}', f'-I{src}', f'-I{work}']
    sources = ('bgreverse.c', 'bgload.c', 'bgdocument.c', 'bgcompile.c', 'bgmaterial.c', 'bgrender.c', 'bghistory.c')
    subprocess.run(cmd + [str(here / 'check.c'), str(here.parent / 'image_import/platform.c')]
                   + [str(src / n) for n in sources] + ['-Wl,--gc-sections', '-lm', '-o', str(work / 'check')], check=True)
    subprocess.run([str(work / 'check'), str(work)], check=True,
                   env=dict(os.environ, ASAN_OPTIONS='detect_leaks=0', UBSAN_OPTIONS='halt_on_error=1'))
    (work / 'context.inc').write_text(helpers.function((src / 'viewport.c').read_text(),
                                                       'ViewportShowGeometryContextMenu'))
    subprocess.run(cmd + [str(here / 'context.c'), '-Wl,--gc-sections', '-lm',
                           '-o', str(work / 'context')], check=True)
    subprocess.run([str(work / 'context')], check=True,
                   env=dict(os.environ, ASAN_OPTIONS='detect_leaks=0', UBSAN_OPTIONS='halt_on_error=1'))
