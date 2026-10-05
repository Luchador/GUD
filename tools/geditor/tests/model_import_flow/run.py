#!/usr/bin/env python3
"""Exercise production import routing and category dialog with Win32 stubs."""
import os
from pathlib import Path
import re
import runpy
import subprocess
import tempfile

here = Path(__file__).resolve().parent
src = here.parents[1] / 'src'
extract = runpy.run_path(str(here.parent / 'model_name_completion/run.py'))['function']
editor = (src / 'modeleditor.c').read_text()
bank = (src / 'newprops.c').read_text()
logic = ''.join(extract(bank, name) for name in ('NewPropsCategory', 'NameValid', 'NewPropsMakeName'))
logic += re.search(r'typedef struct ModelEditorNewModel [^\n]*', editor)[0] + '\n'
logic += ''.join(extract(editor, name) for name in
                 ('ModelEditorTransfer', 'ModelEditorNewModelDialog', 'ModelEditorImportNew'))
resources = (src / 'geditor.rc').read_text()
assert 'Add Prop Model...' not in resources and 'IDC_MODEL_ADD' not in editor
assert 'IDC_NEW_MODEL_CATEGORY' in resources
assert 'EnableWindow(GetDlgItem(g_ModelEditor,IDC_MODEL_IMPORT),g_ModelProject[0]!=0);' in editor
with tempfile.TemporaryDirectory(prefix='geditor-import-flow-') as temporary:
    work = Path(temporary)
    (work / 'logic.inc').write_text(logic)
    subprocess.run([os.environ.get('CC', 'cc'), '-std=c99', '-O1', '-g', '-Wall', '-Wextra',
                    '-Werror', '-Wno-unused-parameter', '-fsanitize=address,undefined',
                    f'-I{work}', f'-I{src}', str(here / 'check.c'), '-o', str(work / 'check')], check=True)
    subprocess.run([str(work / 'check')], check=True,
                   env=dict(os.environ, ASAN_OPTIONS='detect_leaks=0', UBSAN_OPTIONS='halt_on_error=1'))
