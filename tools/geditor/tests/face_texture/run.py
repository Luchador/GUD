#!/usr/bin/env python3
"""Texture assignment on shade-only faces, including an inherited image binding.

Optionally pass an extracted BG from the reported ROM to check faces 4312/4313.
"""
import importlib.util
import os
from pathlib import Path
import subprocess
import sys
import tempfile

sys.dont_write_bytecode = True
HERE = Path(__file__).resolve().parent
SRC = HERE.parents[1] / 'src'


def load(name, path):
    spec = importlib.util.spec_from_file_location(name, path)
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


extract = load('extract', HERE.parent / 'portal_editing/run.py')
native = load('native', HERE.parent / 'bg_transparency/run.py')
with tempfile.TemporaryDirectory(prefix='geditor-face-texture-') as temp:
    work = Path(temp)
    fixture = (HERE.parent / 'bg_transparency/edit.c').read_text()
    (work / 'fixture.inc').write_text(''.join(extract.function(fixture, name) for name in
        ('Put', 'Fixture', 'Refs', 'Equivalent')))
    command = [os.environ.get('CC', 'cc'), '-std=c99', '-O1', '-g', '-Wall', '-Wextra',
        '-Werror', '-Wno-unused-parameter', '-ffunction-sections', '-fdata-sections',
        '-fsanitize=address,undefined', f'-I{HERE.parent / "image_import"}', f'-I{SRC}', f'-I{work}']
    subprocess.run(command + [str(HERE / 'check.c'), str(HERE.parent / 'image_import/platform.c')]
        + [str(SRC / name) for name in ('bgdocument.c', 'bgload.c', 'bgcompile.c',
            'bgmaterial.c', 'bgrender.c', 'bghistory.c')]
        + ['-Wl,--gc-sections', '-lm', '-o', str(work / 'check')], check=True)
    (work / 'depot.seg').write_bytes(native.jungle_fixture(
        (HERE.parents[3] / 'assets/obseg/bg/bg_depo_all_p.c').read_text()))
    subprocess.run([str(work / 'check'), str(work), str(work / 'depot.seg')] + sys.argv[1:],
        check=True, env=dict(os.environ, ASAN_OPTIONS='detect_leaks=0',
                             UBSAN_OPTIONS='halt_on_error=1'))
