#!/usr/bin/env python3
"""Numeric entry uses native data; render-space floats and saved assets stay separate."""
import importlib.util
import os
from pathlib import Path
import re
import subprocess
import tempfile

here = Path(__file__).resolve().parent
src = here.parents[1] / 'src'
root = src.parents[2]
spec = importlib.util.spec_from_file_location('extract', here.parent / 'vertex_snap/run.py')
extract = importlib.util.module_from_spec(spec)
spec.loader.exec_module(extract)
viewport = (src / 'viewport.c').read_text()
with tempfile.TemporaryDirectory(prefix='geditor-units-') as temp:
    work = Path(temp)
    (work / 'types.inc').write_text('\n'.join(re.search(
        r'typedef struct ' + name + r' \{.*?\} ' + name + ';', viewport, re.S)[0]
        for name in ('Vertex', 'ViewportComponent', 'ViewportStanComponent')))
    (work / 'logic.inc').write_text(''.join(extract.function(viewport, name) for name in (
        'ViewportEditorCornerPosition', 'ViewportEditorStanPosition', 'ViewportReadSelectionPosition',
        'ViewportGetSelectionPosition', 'ViewportGetEditorSelectionPosition')))
    scales = re.findall(r'LEVEL\([^\n]*?"-ml[^"]*",\s*([\d.]+)',
                        (root / 'src/game/leveltable.inc').read_text())
    assert len(scales) > 20
    (work / 'scales.inc').write_text(','.join(value + 'f' for value in sorted(set(scales))))
    cmd = [os.environ.get('CC', 'cc'), '-std=c99', '-O1', '-g', '-Wall', '-Wextra', '-Werror',
           '-Wno-unused-parameter', '-ffunction-sections', '-fdata-sections', '-fsanitize=address,undefined',
           f'-I{here.parent / "image_import"}', f'-I{src}', f'-I{root}', f'-I{work}',
           str(here / 'check.c'), str(src / 'bgdocument.c'), '-Wl,--gc-sections', '-lm', '-o', str(work / 'check')]
    subprocess.run(cmd, check=True)
    subprocess.run([str(work / 'check')], check=True,
                   env=dict(os.environ, ASAN_OPTIONS='detect_leaks=0', UBSAN_OPTIONS='halt_on_error=1'))
    settings = (src / 'editorsettings.c').read_text()
    (work / 'preferences.inc').write_text(''.join(extract.function(settings, name) for name in (
        'EditorSettingsLoad', 'EditorSettingsGetUnits', 'EditorSettingsSetUnits')))
    cmd[cmd.index(str(here / 'check.c'))] = str(here / 'preferences.c')
    cmd.remove(str(src / 'bgdocument.c'))
    subprocess.run(cmd, check=True)
    subprocess.run([str(work / 'check')], check=True,
                   env=dict(os.environ, ASAN_OPTIONS='detect_leaks=0', UBSAN_OPTIONS='halt_on_error=1'))
