#!/usr/bin/env python3
"""Baked lighting: production normals, native BG export and editor undo/rollback."""
import importlib.util
import os
from pathlib import Path
import subprocess
import tempfile

here = Path(__file__).resolve().parent
src = here.parents[1] / 'src'
shim = here.parent / 'image_import'
spec = importlib.util.spec_from_file_location('extract', here.parent / 'bg_primitives/run.py')
helpers = importlib.util.module_from_spec(spec)
spec.loader.exec_module(helpers)
with tempfile.TemporaryDirectory(prefix='geditor-baked-lighting-') as folder:
    work = Path(folder)
    fixture = (here.parent / 'bg_primitives/check.c').read_text()
    (work/'fixture.inc').write_text(''.join(helpers.function(fixture, name) for name in
        ('Put', 'Float', 'Fixture', 'RoundTrip')))
    (work/'editor.inc').write_text(helpers.function((src/'geditor.c').read_text(), 'GEditorBakeLighting'))
    command = [os.environ.get('CC','cc'), '-std=c99', '-O1', '-g', '-Wall', '-Wextra', '-Werror',
        '-Wno-unused-parameter', '-ffunction-sections', '-fdata-sections', '-fsanitize=address,undefined',
        f'-I{shim}', f'-I{src}', f'-I{work}', str(here/'check.c'), str(shim/'platform.c')]
    command += [str(src/name) for name in ('bgload.c','bgdocument.c','bgcompile.c','bgmaterial.c','bgrender.c','bghistory.c')]
    subprocess.run(command+['-Wl,--gc-sections','-lm','-o',str(work/'check')], check=True)
    subprocess.run([str(work/'check'),str(work)], check=True,
        env=dict(os.environ,ASAN_OPTIONS='detect_leaks=0',UBSAN_OPTIONS='halt_on_error=1'))
    ui = (src/'bakedlighting.c').read_text().replace(' CALLBACK ', ' ')
    (work/'ui.inc').write_text(''.join(helpers.function(ui, name) for name in
        ('Number','ReadNumber','Settings','UpdateButtons','AddRoom','Dialog','BakedLightingRefresh')))
    command = command[:command.index(str(here/'check.c'))]
    subprocess.run(command+[str(here/'ui.c'),str(src/'bglighting.c'),str(shim/'platform.c'),
        '-Wl,--gc-sections','-lm','-o',str(work/'ui')], check=True)
    subprocess.run([str(work/'ui')], check=True,
        env=dict(os.environ,ASAN_OPTIONS='detect_leaks=0',UBSAN_OPTIONS='halt_on_error=1'))
