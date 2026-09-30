#!/usr/bin/env python3
"""Selected model-face texture edits through native compilation and ROM export."""
import importlib.util
import os
from pathlib import Path
import subprocess
import sys
import tempfile

sys.dont_write_bytecode = True
here = Path(__file__).resolve().parent
src = here.parents[1] / 'src'
root = src.parents[2]

def module(name, path):
    spec = importlib.util.spec_from_file_location(name, path)
    result = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(result)
    return result

with tempfile.TemporaryDirectory(prefix='geditor-face-texture-') as folder:
    work = Path(folder)
    command = [os.environ.get('CC', 'cc'), '-std=c99', '-O1', '-g', '-Wall', '-Wextra',
        '-Werror', '-Wno-unused-parameter', '-Dfopen=TestFopen', '-ffunction-sections', '-fdata-sections',
        '-fsanitize=address,undefined', f'-I{here.parent / "image_import"}', f'-I{src}', f'-I{root}',
        str(here / 'check.c'), str(here.parent / 'image_import/platform.c')]
    command += [str(src / n) for n in ('modelload.c', 'modelmaterials.c', 'modelcompile.c',
        'modeledits.c', 'gltf.c', 'gltfjson.c', 'newprops.c', 'propcompile.c', 'bgmaterial.c', 'bgrender.c')]
    subprocess.run(command + ['-Wl,--gc-sections', '-lm', '-o', str(work / 'check')], check=True)
    fixtures = module('fixtures', here.parent / 'model_vertex_uv/run.py')
    assets = [root / 'assets/obseg/chr/CtrevguardZ.bin', root / 'assets/obseg/prop/Pbook1Z.bin']
    for kind in ('mixed', 'normals', 'reflection'):
        path = work / f'{kind}.bin'
        if kind == 'mixed': fixtures.mixed_fixture(path)
        else: fixtures.color_fixture(path, kind)
        assets.append(path)
    props = module('props', here.parent / 'new_props/run.py')
    custom = work / 'custom.glb'
    props.fixture(custom)
    env = dict(os.environ, ASAN_OPTIONS='detect_leaks=0', UBSAN_OPTIONS='halt_on_error=1')
    for i, asset in enumerate(assets + [assets[1]]):
        project = work / f'project{i}'
        (project / 'models' / ('characters' if asset.stem.startswith('C') else 'objects')).mkdir(parents=True)
        is_custom = i == len(assets)
        name = asset.stem if asset.stem.startswith(('C', 'P')) else f'P{asset.stem}Z'
        subprocess.run([str(work / 'check'), str(asset), str(project), 'PcustomZ' if is_custom else name]
            + ([str(custom)] if is_custom else []), env=env, check=True)
