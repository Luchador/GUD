#!/usr/bin/env python3
"""Studio transforms: production persistence/math/dragging and actual gizmo assets.

The Win32 resource calls use fixture bytes; production gizmo geometry and picking
functions are extracted unchanged. Native window interaction is not simulated.
"""
import importlib.util
import os
from pathlib import Path
import subprocess
import tempfile


def extract(source, name):
    marker = source.index(name + '(')
    start = source.rfind('\n', 0, marker) + 1
    end = source.index('{', marker) + 1
    depth = 1
    while depth:
        depth += (source[end] == '{') - (source[end] == '}')
        end += 1
    return source[start:end]


def main():
    here = Path(__file__).resolve().parent
    src = here.parent.parent / 'src'
    shim = here.parent / 'project_rebase'
    spec = importlib.util.spec_from_file_location('fixture', here.parent / 'studio_materials/run.py')
    fixtures = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(fixtures)
    with tempfile.TemporaryDirectory(prefix='geditor-studio-transforms-') as tmp:
        work = Path(tmp)
        (work / 'studio/models').mkdir(parents=True)
        (work / 'studio/scenes').mkdir()
        fixtures.fixture(work / 'studio/models')
        source = (src / 'studiogizmo.c').read_text()
        (work / 'gizmo.inc').write_text('\n'.join(extract(source, name) for name in (
            'StudioGizmoFree', 'StudioGizmoLoad', 'StudioGizmoCount', 'StudioGizmoVertex', 'StudioGizmoPick')))
        command = [os.environ.get('CC', 'cc'), '-O1', '-g', '-std=c99', '-Wall', '-Wextra', '-Werror',
                   '-Wno-unused-parameter', '-Wno-format', '-ffunction-sections', '-fdata-sections',
                   '-fsanitize=address,undefined', '-Dfopen=TestFopen', f'-I{shim}', f'-I{src}', f'-I{work}',
                   f'-I{src.parents[2]}', str(here / 'check.c'), str(shim / 'platform.c')]
        command += [str(src / n) for n in ('studiodocument.c', 'studiomath.c', 'studiodrag.c', 'orbitcamera.c',
                                          'rotation.c', 'gltf.c', 'gltfjson.c', 'modelmaterials.c', 'bgmaterial.c', 'bgrender.c')]
        command += ['-Wl,--gc-sections', '-lm', '-o', str(work / 'check')]
        subprocess.run(command, check=True)
        subprocess.run([str(work / 'check'), str(work), str(src.parent / 'geditorassets')], check=True,
                       env=dict(os.environ, ASAN_OPTIONS='detect_leaks=0', UBSAN_OPTIONS='halt_on_error=1'))
        source = (src / 'renderstudio.c').read_text()
        (work / 'input.inc').write_text('\n'.join(extract(source, name) for name in (
            'RenderStudioSaveScene', 'RenderStudioHandleMessage')))
        subprocess.run([os.environ.get('CC', 'cc'), '-std=c99', '-Wall', '-Wextra', '-Werror',
                        '-Wno-unused-parameter', '-fsanitize=address,undefined', f'-I{src}', f'-I{work}',
                        str(here / 'input.c'), '-o', str(work / 'input')], check=True)
        subprocess.run([str(work / 'input')], check=True,
                       env=dict(os.environ, ASAN_OPTIONS='detect_leaks=0', UBSAN_OPTIONS='halt_on_error=1'))
        source = (src / 'studioviewport.c').read_text()
        (work / 'transaction.inc').write_text('\n'.join(extract(source, name) for name in (
            'StudioViewportEndTransform', 'StudioViewportCancelTransform', 'StudioViewportCommitTransform')))
        subprocess.run([os.environ.get('CC', 'cc'), '-std=c99', '-Wall', '-Wextra', '-Werror',
                        '-Wno-unused-parameter', '-fsanitize=address,undefined', f'-I{shim}', f'-I{src}', f'-I{work}',
                        str(here / 'transaction.c'), '-o', str(work / 'transaction')], check=True)
        subprocess.run([str(work / 'transaction')], check=True,
                       env=dict(os.environ, ASAN_OPTIONS='detect_leaks=0', UBSAN_OPTIONS='halt_on_error=1'))


if __name__ == '__main__':
    main()
