#!/usr/bin/env python3
"""Real native head/body round trips and character bank regression (ASan/UBSan).
ROM directory and texture image decoding are mocked; native model data,
compiler, GLB, edit store, bank export, character lookup and posing are real.
"""
import os
import runpy
from pathlib import Path
import subprocess
import tempfile
here=Path(__file__).resolve().parent
src=here.parents[1]/'src'
root=src.parents[2]
with tempfile.TemporaryDirectory(prefix='geditor-characters-') as tmp:
    work=Path(tmp)
    (work/'models/characters').mkdir(parents=True)
    runpy.run_path(str(here.parent/'new_props/run.py'))['fixture'](work/'prop.glb')
    command=[os.environ.get('CC','cc'),'-std=c99','-O1','-g','-Wall','-Wextra','-Werror',
        '-Wno-unused-parameter','-ffunction-sections','-fdata-sections','-fsanitize=address,undefined',
        '-Dfopen=TestFopen',f'-I{here.parent/"image_import"}',f'-I{src}',f'-I{root}',
        str(here/'check.c'),str(here.parent/'image_import/platform.c')]
    command += [str(src/name) for name in ('newprops.c','propcompile.c','modelcompile.c','modelload.c',
        'modeledits.c','modelmaterials.c','gltf.c','gltfjson.c','bgmaterial.c','bgrender.c',
        'characterload.c','objectload.c','objectshade.c')]
    subprocess.run(command+['-Wl,--gc-sections','-lm','-o',str(work/'check')],check=True)
    subprocess.run([str(work/'check'),str(work),str(root)],check=True,
        env=dict(os.environ,ASAN_OPTIONS='detect_leaks=0',UBSAN_OPTIONS='halt_on_error=1'))

    runtime=(root/'src/game/pobjdata.c').read_text()
    runtime=runtime[runtime.index('CustomPropRomConfig g_CustomPropRomConfig'):runtime.index('ExplosionDetailsRecord *propExplosionGet')]
    runtime=runtime.replace('(u32)scratch','(uintptr_t)scratch')
    (work/'runtime.inc').write_text(runtime)
    subprocess.run([os.environ.get('CC','cc'),'-std=c99','-O1','-g','-Wall','-Wextra',
        '-Wno-unused-parameter','-Wno-int-to-pointer-cast','-fsanitize=address,undefined',
        f'-I{work}',f'-I{root}',str(here/'runtime.c'),'-o',str(work/'runtime')],check=True)
    subprocess.run([str(work/'runtime')],check=True,
        env=dict(os.environ,ASAN_OPTIONS='detect_leaks=0',UBSAN_OPTIONS='halt_on_error=1'))
