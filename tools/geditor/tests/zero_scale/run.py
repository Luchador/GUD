#!/usr/bin/env python3
"""Panel input through geometry scaling, native persistence and editor history."""
import importlib.util
import os
from pathlib import Path
import subprocess
import sys
import tempfile

here = Path(__file__).resolve().parent
src = here.parents[1] / 'src'
shim = here.parent / 'image_import'
sys.dont_write_bytecode = True
spec = importlib.util.spec_from_file_location('extract', here.parent / 'view_modes/run.py')
extract = importlib.util.module_from_spec(spec)
spec.loader.exec_module(extract)
with tempfile.TemporaryDirectory(prefix='geditor-zero-scale-') as temp:
    work = Path(temp)
    fixture = (here.parent / 'bg_transparency/edit.c').read_text()
    (work / 'fixture.inc').write_text(''.join(extract.function(fixture, n) for n in
        ('Put', 'Fixture', 'Refs', 'Equivalent')))
    editor = (src / 'geditor.c').read_text()
    (work / 'editor.inc').write_text(extract.function(editor, 'GEditorTransformSelection'))
    start = editor.index('    case RIGHTPANEL_WM_SET_SCALE:')
    end = editor.index('    case VIEWPORT_WM_SCALE_SELECTION:', start)
    (work / 'dispatch.inc').write_text('static BOOL DispatchScale(HWND hwnd, LPARAM lparam)\n'
        + editor[start:end].split(':', 1)[1])
    panel = (src / 'rightpanel.c').read_text()
    (work / 'panel.inc').write_text(''.join(extract.function(panel, n) for n in
        ('RightPanelParseTransformValue', 'RightPanelSetPosition')))
    command = [os.environ.get('CC', 'cc'), '-std=c99', '-O1', '-g', '-Wall', '-Wextra', '-Werror',
        '-Wno-unused-parameter', '-ffunction-sections', '-fdata-sections', '-fsanitize=address,undefined',
        f'-I{shim}', f'-I{src}', f'-I{work}', str(here / 'check.c'), str(shim / 'platform.c')]
    command += [str(src / n) for n in ('bgload.c', 'bgdocument.c', 'bgcompile.c', 'bgmaterial.c',
        'bgrender.c', 'bguv.c', 'bghistory.c', 'stanload.c', 'stanedit.c', 'standelete.c',
        'stantopology.c', 'stanquery.c', 'rotation.c', 'scaling.c')]
    subprocess.run(command + ['-Wl,--gc-sections', '-lm', '-o', str(work / 'check')], check=True)
    subprocess.run([str(work / 'check')], check=True,
        env=dict(os.environ, ASAN_OPTIONS='detect_leaks=0', UBSAN_OPTIONS='halt_on_error=1'))
