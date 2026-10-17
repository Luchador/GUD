#!/usr/bin/env python3
"""Test production slot storage and project persistence without a game ROM."""
import os
import re
from pathlib import Path
import subprocess
import tempfile

here = Path(__file__).resolve().parent
src = here.parents[1] / 'src'
with tempfile.TemporaryDirectory(prefix='geditor-level-slots-') as folder:
    work = Path(folder)
    command = [os.environ.get('CC', 'cc'), '-std=c99', '-O1', '-g', '-Wall', '-Wextra', '-Werror',
        '-Wno-unused-parameter', '-ffunction-sections', '-fdata-sections', '-fsanitize=address,undefined',
        '-Dfopen=TestFopen', f'-I{here.parent / "image_import"}', f'-I{src}',
        str(here / 'check.c'), str(here.parent / 'image_import/platform.c')]
    command += [str(src / name) for name in ('levelslots.c', 'project.c', 'environment.c', 'levelmemory.c', 'fog.c')]
    subprocess.run(command + ['-Wl,--gc-sections', '-lm', '-o', str(work / 'check')], check=True)
    subprocess.run([str(work / 'check'), folder], check=True,
        env=dict(os.environ, ASAN_OPTIONS='detect_leaks=0', UBSAN_OPTIONS='halt_on_error=1'))

    panel = (src / 'projectsettings.c').read_text()
    def function(name):
        m = re.search(r'^[\w *]+\b' + name + r'\([^;]*?\)\s*\{', panel, re.M)
        assert m, name
        end = panel.index('{', m.start()) + 1
        depth = 1
        while depth:
            depth += (panel[end] == '{') - (panel[end] == '}')
            end += 1
        return panel[m.start():end] + '\n'
    events = function('ProjectSettingsEndLevelDrag') + function('ProjectSettingsSlotAt')
    events += 'static int Event(HWND hwnd,int message,unsigned wparam,unsigned lparam) { switch(message) {\n'
    events += panel[panel.index('    case WM_MOUSEMOVE:'):panel.index('    case WM_CLOSE:')]
    events += '}return FALSE;}\n'
    (work / 'events.inc').write_text(events)
    subprocess.run([os.environ.get('CC', 'cc'), '-std=c99', '-Wall', '-Wextra', '-Werror',
        '-fsanitize=address,undefined', f'-I{work}', str(here / 'ui.c'), '-o', str(work / 'ui')], check=True)
    subprocess.run([str(work / 'ui')], check=True,
        env=dict(os.environ, ASAN_OPTIONS='detect_leaks=0', UBSAN_OPTIONS='halt_on_error=1'))
