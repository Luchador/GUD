#!/usr/bin/env python3
"""Offset dialog Apply/Reset/history behavior with production control logic."""
import importlib.util
import os
from pathlib import Path
import re
import subprocess
import tempfile

here = Path(__file__).resolve().parent
src = here.parents[1] / 'src'
spec = importlib.util.spec_from_file_location('extract', here.parent / 'object_properties/run.py')
extract = importlib.util.module_from_spec(spec)
spec.loader.exec_module(extract)
source = (src / 'headoffset.c').read_text()
with tempfile.TemporaryDirectory(prefix='geditor-head-offset-') as tmp:
    work = Path(tmp)
    types = source[source.index('typedef struct HeadOffsetStep'):source.index('static void HeadOffsetFields')]
    (work / 'types.inc').write_text(types)
    (work / 'logic.inc').write_text(''.join(extract.function(source, name) for name in
        ('HeadOffsetFields', 'HeadOffsetRead', 'HeadOffsetNotify', 'HeadOffsetApply', 'HeadOffsetUndo')))
    subprocess.run([os.environ.get('CC', 'cc'), '-std=c99', '-O1', '-g', '-Wall', '-Wextra', '-Werror',
        '-Wno-unused-parameter', '-fsanitize=address,undefined', f'-I{here.parent / "image_import"}',
        f'-I{src}', f'-I{work}', str(here / 'check.c'), '-o', str(work / 'check')], check=True)
    subprocess.run([str(work / 'check')], check=True,
        env=dict(os.environ, ASAN_OPTIONS='detect_leaks=0', UBSAN_OPTIONS='halt_on_error=1'))
    # The button must cross the intermediate panel before reaching the frame.
    panel = (src / 'rightpanel.c').read_text()
    relay = re.search(r'(?:    case [A-Z_]+:\n)+        return SendMessage\(GetParent\(hwnd\), msg, wparam, lparam\);', panel)[0]
    assert 'case CHARACTERPROPERTIES_WM_HEAD_OFFSET:' in relay
    editor = (src / 'geditor.c').read_text()
    assert editor.count('!HeadOffsetHandleMessage(&msg)') == 2
    assert 'case CHARACTERPROPERTIES_WM_HEAD_OFFSET:' in editor
    assert 'HeadOffsetClose();' in extract.function(editor, 'GEditorCloseProject')
