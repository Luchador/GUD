#!/usr/bin/env python3
"""Exercise project rebase and real ROM/model/image export on a POSIX file shim.

No ROM download is needed. The fixture has a current GUD manifest, relocated
tables, a real repository model, native textures and editable level resources.
The Windows dialog itself is checked by the MinGW build, not this harness.
"""
import importlib.util
import os
from pathlib import Path
import subprocess
import sys
import tempfile


def main():
    here = Path(__file__).resolve().parent
    src = here.parent.parent / 'src'
    root = src.parents[2]
    spec = importlib.util.spec_from_file_location('image_tests', here.parent / 'image_import/run.py')
    image_tests = importlib.util.module_from_spec(spec)
    sys.dont_write_bytecode = True
    spec.loader.exec_module(image_tests)
    texture = (src / 'texload.c').read_text()
    native = '#include <stdio.h>\n#include <stdlib.h>\n#include <string.h>\n#include "texload.h"\n#include "imageedits.h"\n'
    native += texture[texture.index('#define GUTX_DESC_OFFSET'):texture.index('/* WIC handles')]
    native = native.replace(image_tests.function(texture, 'TexRestoreImportBmpAlpha'), '')
    for name in ('TexWriteBmp', 'TexLoadSavedProjectImage', 'TexLoadProjectImage', 'TexGetProjectImageSize'):
        native += '\n' + image_tests.function(texture, name) + '\n'
    with tempfile.TemporaryDirectory(prefix='geditor-rebase-') as temp:
        work = Path(temp)
        (work / 'texture.c').write_text(native)
        command = [os.environ.get('CC', 'cc'), '-O1', '-g', '-std=c99', '-Wall', '-Wextra',
                   '-Wno-unused-parameter', '-Wno-format', '-ffunction-sections', '-fdata-sections',
                   '-fsanitize=address,undefined', '-Dfopen=TestFopen', f'-I{here}', f'-I{src}', f'-I{root}',
                   str(here / 'check.c'), str(here / 'platform.c'), str(work / 'texture.c')]
        command += [str(src / name) for name in ('occluders.c', 'projectrebase.c', 'project.c', 'levelmemory.c', 'environment.c', 'fog.c', 'rom.c', 'romexport.c', 'levelissues.c',
                   'texrom.c', 'texinfo.c', 'texencode.c', 'imageedits.c', 'modeledits.c', 'modelload.c', 'modelmaterials.c',
                   'setupload.c', 'setupstan.c', 'stanload.c', 'stanquery.c', 'actionblocks.c', 'gltf.c', 'bgrender.c', 'bgcompile.c', 'bgdocument.c', 'bgload.c', 'modelcompile.c', 'bgmaterial.c', 'newprops.c', 'propcompile.c')]
        command += [str(root / 'src/game/occlusionmath.c')]
        command += ['-Wl,--gc-sections', '-lm', '-o', str(work / 'check')]
        subprocess.run(command, check=True)
        env = dict(os.environ, ASAN_OPTIONS='detect_leaks=0', UBSAN_OPTIONS='halt_on_error=1')
        spec = importlib.util.spec_from_file_location('new_props', here.parent / 'new_props/run.py')
        new_props = importlib.util.module_from_spec(spec)
        spec.loader.exec_module(new_props)
        model = work / 'pendant.glb'
        doc, data = new_props.fixture(model)
        doc['images'][0]['name'] = 'GUD Image 0000'
        doc['images'][1]['name'] = 'GUD Image 0001'
        new_props.write(model, doc, data)
        subprocess.run([str(work / 'check'), str(work), str(root / 'assets/obseg/prop/Pjungle3_treeZ.bin'), str(model)],
                       env=env, check=True)


if __name__ == '__main__':
    main()
