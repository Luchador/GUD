#!/usr/bin/env python3
"""Stage budgets: native slots, persistence, validation and the real form/controller."""
import importlib.util
import os
from pathlib import Path
import subprocess
import sys
import tempfile

here = Path(__file__).resolve().parent
src = here.parents[1] / 'src'
sys.dont_write_bytecode = True
spec = importlib.util.spec_from_file_location('extract', here.parent / 'zoom_selected/run.py')
extract = importlib.util.module_from_spec(spec)
spec.loader.exec_module(extract)
with tempfile.TemporaryDirectory(prefix='geditor-memory-') as folder:
    work = Path(folder)
    panel = (src / 'stageoptions.c').read_text()
    editor = (src / 'geditor.c').read_text()
    (work / 'ui_types.inc').write_text(panel[panel.index('enum { MEMORY_HEADING'):panel.index('static StageOptions *State')])
    (work / 'ui.inc').write_text(extract.function(editor, 'GEditorApplyStageOptions') + ''.join(
        extract.function(panel, name) for name in ('State', 'Owner', 'ReadFields', 'Status', 'Load', 'Commit',
            'StageOptionsApply', 'StageOptionsHasDraft', 'StageOptionsRefresh')))
    command = [os.environ.get('CC', 'cc'), '-std=c99', '-O1', '-g', '-Wall', '-Wextra', '-Werror',
        '-Wno-unused-parameter', '-ffunction-sections', '-fdata-sections', '-fsanitize=address,undefined',
        '-Dfopen=TestFopen', f'-I{here.parent / "image_import"}', f'-I{src}', f'-I{work}',
        str(here / 'check.c'), str(here.parent / 'image_import/platform.c')]
    command += [str(src / name) for name in ('levelmemory.c', 'project.c', 'environment.c', 'fog.c', 'rom.c')]
    subprocess.run(command + ['-Wl,--gc-sections', '-lm', '-o', str(work / 'check')], check=True)
    subprocess.run([str(work / 'check'), str(work)], check=True,
        env=dict(os.environ, ASAN_OPTIONS='detect_leaks=0', UBSAN_OPTIONS='halt_on_error=1'))
