#!/usr/bin/env python3
"""Native LOD material edits, undo and ROM replacements under ASan/UBSan."""
import os
from pathlib import Path
import subprocess
import tempfile

here = Path(__file__).resolve().parent
src = here.parents[1] / 'src'
root = src.parents[2]
with tempfile.TemporaryDirectory(prefix='geditor-model-lods-') as folder:
    work = Path(folder)
    command = [os.environ.get('CC', 'cc'), '-std=c99', '-O1', '-g', '-Wall', '-Wextra',
        '-Werror', '-Wno-unused-parameter', '-Dfopen=TestFopen', '-ffunction-sections', '-fdata-sections',
        '-fsanitize=address,undefined', f'-I{here.parent / "image_import"}', f'-I{src}', f'-I{root}',
        str(here / 'check.c'), str(here.parent / 'image_import/platform.c')]
    command += [str(src / n) for n in ('modelload.c', 'modelmaterials.c', 'modelcompile.c',
        'modeledits.c', 'gltf.c', 'newprops.c', 'propcompile.c', 'bgmaterial.c', 'bgrender.c')]
    subprocess.run(command + ['-Wl,--gc-sections', '-lm', '-o', str(work / 'check')], check=True)
    env = dict(os.environ, ASAN_OPTIONS='detect_leaks=0', UBSAN_OPTIONS='halt_on_error=1')
    for name, kind in [('CtrevguardZ', 'chr'), ('Pbook1Z', 'prop')]:
        project = work / name
        (project / 'models' / ('characters' if kind == 'chr' else 'objects')).mkdir(parents=True)
        subprocess.run([str(work / 'check'), str(root / f'assets/obseg/{kind}/{name}.bin'),
            str(project), name], env=env, check=True)
