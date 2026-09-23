#!/usr/bin/env python3
"""Environment mapping: editor compilation through runtime display lists."""
import importlib.util
import os
from pathlib import Path
import re
import subprocess
import sys
import tempfile

HERE = Path(__file__).resolve().parent
SRC = HERE.parents[1] / 'src'
sys.dont_write_bytecode = True

def load(name, path):
    spec = importlib.util.spec_from_file_location(name, path)
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module

extract = load('extract', HERE.parent / 'portal_editing/run.py')
native = load('native', HERE.parent / 'bg_transparency/run.py')
with tempfile.TemporaryDirectory(prefix='geditor-face-environment-') as temp:
    work = Path(temp)
    viewport = (SRC / 'viewport.c').read_text()
    (work / 'viewport_types.inc').write_text(''.join(re.search(
        r'typedef struct ' + name + r' \{.*?\} ' + name + ';', viewport, re.S)[0] + '\n'
        for name in ('Vertex', 'VertexColor', 'SceneBatch')))
    (work / 'viewport_functions.inc').write_text(''.join(extract.function(viewport, name)
        for name in ('ViewportEnvironmentCoordinates', 'ViewportRefreshBgVertexColor')))
    fixture = (HERE.parent / 'bg_transparency/edit.c').read_text()
    (work / 'fixture.inc').write_text(''.join(extract.function(fixture, n) for n in ('Put', 'Fixture', 'Refs')))
    batching = (HERE.parent / 'texture_batching/check.c').read_text()
    comparison = re.search(r'typedef struct FaceKey\s*\{.*?\} FaceKey;', batching, re.S)[0]
    comparison += '\n' + ''.join(extract.function(batching, n) for n in ('Key', 'Compare', 'EquivalentDocuments'))
    (work / 'comparison.inc').write_text(comparison.replace('OK(', 'assert('))
    (work / 'runway.seg').write_bytes(native.jungle_fixture(
        (HERE.parents[3] / 'assets/obseg/bg/bg_run_all_p.c').read_text()))
    command = [os.environ.get('CC', 'cc'), '-std=c99', '-O1', '-g', '-Wall', '-Wextra',
        '-Werror', '-Wno-unused-parameter', '-ffunction-sections', '-fdata-sections',
        '-fsanitize=address,undefined', f'-I{HERE.parent / "image_import"}', f'-I{SRC}', f'-I{work}',
        str(HERE / 'check.c'), str(HERE.parent / 'image_import/platform.c')]
    command += [str(SRC / n) for n in ('bgdocument.c', 'bgload.c', 'bgcompile.c', 'bgmaterial.c', 'bgrender.c', 'bghistory.c', 'bgroom.c', 'rotation.c', 'scaling.c')]
    subprocess.run(command + ['-Wl,--gc-sections', '-lm', '-o', str(work / 'check')], check=True)
    subprocess.run([str(work / 'check'), str(work / 'environment.bin'), str(work / 'runway.seg')], check=True,
        env=dict(os.environ, ASAN_OPTIONS='detect_leaks=0', UBSAN_OPTIONS='halt_on_error=1'))

    subprocess.run([sys.executable, str(HERE.parents[2] / 'tests/bg_onecycle/run.py')], check=True,
        env=dict(os.environ, GUD_ENV_FIXTURE=str(work / 'environment.bin')))
