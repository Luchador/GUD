#!/usr/bin/env python3
"""Exercise the level picker, navigation guards and reclaimed browser layout."""
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
spec = importlib.util.spec_from_file_location('extract', here.parent / 'zoom_selected/run.py')
extract = importlib.util.module_from_spec(spec)
spec.loader.exec_module(extract)
editor = (src / 'geditor.c').read_text()
browser = (src / 'browser.c').read_text()
resource = (src / 'geditor.rc').read_text()
template = resource.split('IDD_OPEN_LEVEL DIALOGEX', 1)[1].split('END', 1)[0]
assert 'LBS_SORT' in template and 'LBS_NOTIFY' in template
assert '"&Open", IDOK' in template and '"Cancel", IDCANCEL' in template
new_level = editor.index('AppendMenu(filemenu, MF_STRING, ID_FILE_NEW_LEVEL,')
open_level = editor.index('AppendMenu(filemenu, MF_STRING, ID_FILE_OPEN_LEVEL,')
separator = editor.index('AppendMenu(filemenu, MF_SEPARATOR', new_level)
assert new_level < open_level < separator
assert 'BROWSER_SECTION_LEVELS' not in browser and 'BrowserSetLevels' not in editor
types = re.search(r'typedef struct GEditorOpenLevelDialog \{.*?\} GEditorOpenLevelDialog;', editor, re.S)[0]
types += '\n' + '\n'.join(re.findall(r'^#define BROWSER_(?:SECTION_\w+|HEADER_H|OBJECT_TAB_H) .*', browser, re.M))
logic = '\n'.join(extract.function(editor, name) for name in
    ('GEditorOpenLevelDialogProc', 'GEditorPromptForOpenLevel'))
logic += '\n' + extract.function(browser, 'BrowserLayoutSections')
with tempfile.TemporaryDirectory(prefix='geditor-open-level-') as temp:
    work = Path(temp)
    (work / 'types.inc').write_text(types)
    (work / 'logic.inc').write_text(logic)
    subprocess.run([os.environ.get('CC', 'cc'), '-std=c99', '-O1', '-g', '-Wall', '-Wextra',
        '-Werror', '-Wno-unused-parameter', '-fsanitize=address,undefined',
        f'-I{work}', str(here / 'check.c'), '-o', str(work / 'check')], check=True)
    subprocess.run([str(work / 'check')], check=True,
        env=dict(os.environ, ASAN_OPTIONS='detect_leaks=0', UBSAN_OPTIONS='halt_on_error=1'))
