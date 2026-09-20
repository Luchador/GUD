#!/usr/bin/env python3
"""Read-only level diagnostics and modeless navigation. Optional: Depot audit directory."""
import importlib.util
import os
from pathlib import Path
import re
import shutil
import subprocess
import sys
import tempfile

here = Path(__file__).resolve().parent
src = here.parents[1] / 'src'
sys.dont_write_bytecode = True
spec = importlib.util.spec_from_file_location('extract', here.parent / 'zoom_selected/run.py')
extract = importlib.util.module_from_spec(spec)
spec.loader.exec_module(extract)
editor = (src / 'geditor.c').read_text()
ui = (src / 'issueswindow.c').read_text()
assert editor.count('!IssuesWindowHandleMessage(&msg)') == 2
for name in ('GEditorCloseProject', 'GEditorDispatchMessage'):
    assert 'IssuesWindowClose();' in extract.function(editor, name)
proc = extract.function(editor, 'GEditorWndProc')
assert 'setup != g_EditHistory.currentsetuprevision' in proc
assert 'stan != g_EditHistory.currentstanrevision' in proc
assert 'bg != g_EditHistory.currentbgrevision' in proc
with tempfile.TemporaryDirectory(prefix='geditor-issues-') as folder:
    work = Path(folder)
    (work / 'navigation.inc').write_text(extract.function(editor, 'GEditorLocateIssue'))
    (work / 'ui_type.inc').write_text(re.search(r'typedef struct IssuesWindow \{.*?\} IssuesWindow;', ui, re.S)[0])
    (work / 'ui_logic.inc').write_text(''.join(extract.function(ui, n) for n in
        ('State', 'Selected', 'Details', 'Clear', 'Check', 'Locate', 'IssuesWindowInvalidate', 'IssuesWindowHandleMessage')))
    args = []
    if len(sys.argv) > 1:
        audit = Path(sys.argv[1])
        (work / 'setup').mkdir()
        shutil.copyfile(audit / 'rom-depot.set', work / 'setup/UsetupdepoZ.set')
        shutil.copyfile(audit / 'rom-depot.stan', work / 'depot.stan')
        shutil.copyfile(audit / 'rom-depot.seg', work / 'depot.seg')
        args = [str(work)]
    command = [os.environ.get('CC', 'cc'), '-std=c99', '-O1', '-g', '-Wall', '-Wextra', '-Werror',
        '-Wno-unused-parameter', '-Wno-format', '-ffunction-sections', '-fdata-sections',
        '-fsanitize=address,undefined', f'-I{here.parent / "image_import"}', f'-I{src}', f'-I{src.parents[2]}', f'-I{work}',
        str(here / 'check.c'), str(here.parent / 'image_import/platform.c')]
    command += [str(src / n) for n in ('levelissues.c', 'stanload.c', 'stanquery.c', 'setupload.c',
        'patrolpaths.c', 'actionblocks.c', 'bgdocument.c', 'bgload.c', 'bgmaterial.c', 'bgrender.c')]
    subprocess.run(command + ['-Wl,--gc-sections', '-lm', '-o', str(work / 'check')], check=True)
    subprocess.run([str(work / 'check')] + args, check=True,
        env=dict(os.environ, ASAN_OPTIONS='detect_leaks=0', UBSAN_OPTIONS='halt_on_error=1'))
