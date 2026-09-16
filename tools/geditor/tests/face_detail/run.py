#!/usr/bin/env python3
"""Check native detail markers and mixed face selections without a Windows GUI."""
import importlib.util
import os
from pathlib import Path
import re
import subprocess
import tempfile


def main():
    here = Path(__file__).resolve().parent
    src = here.parents[1] / 'src'
    spec = importlib.util.spec_from_file_location('extract', here.parent / 'bg_selection/run.py')
    helpers = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(helpers)
    source = (src / 'faceproperties.c').read_text()
    logic = re.search(r'typedef struct FaceDetailSelection\s*\{.*?\} FaceDetailSelection;', source, re.S)[0]
    logic += '\n' + ''.join(helpers.function(source, name) for name in
                           ('FacePropertiesGetDetail', 'FacePropertiesDetailScale', 'FacePropertiesParseDetailNumber'))
    with tempfile.TemporaryDirectory(prefix='geditor-face-detail-') as temp:
        work = Path(temp)
        (work / 'selection.inc').write_text(logic)
        subprocess.run([os.environ.get('CC', 'cc'), '-std=c99', '-O1', '-g', '-Wall', '-Wextra',
                        '-Werror', '-fsanitize=address,undefined',
                        f'-I{here.parent / "image_import"}', f'-I{src}', f'-I{work}',
                        str(here / 'check.c'), str(src / 'bgmaterial.c'),
                        '-o', str(work / 'check')], check=True)
        subprocess.run([str(work / 'check')], check=True,
                       env=dict(os.environ, ASAN_OPTIONS='detect_leaks=0', UBSAN_OPTIONS='halt_on_error=1'))

        (work / 'validation.inc').write_text(helpers.function((src / 'geditor.c').read_text(), 'GEditorValidateDetailImages'))
        spec = importlib.util.spec_from_file_location('fixture' , here.parent / 'bg_transparency/run.py')
        fixture = importlib.util.module_from_spec(spec)
        spec.loader.exec_module(fixture)
        native = []
        for name in ('depo', 'tra', 'dish', 'sevb'):
            path = work / (name + '.seg')
            path.write_bytes(fixture.jungle_fixture((here.parents[3] / ('assets/obseg/bg/bg_' + name + '_all_p.c')).read_text()))
            native.append(str(path))
        subprocess.run([os.environ.get('CC', 'cc'), '-std=c99', '-O1', '-g', '-Wall', '-Wextra',
                        '-Werror', '-Wno-unused-parameter', '-ffunction-sections', '-fdata-sections',
                        '-fsanitize=address,undefined', f'-I{here.parent / "image_import"}', f'-I{src}', f'-I{work}',
                        str(here / 'edit.c'), str(here.parent / 'image_import/platform.c')]
                       + [str(src / name) for name in ('bgload.c', 'bgcompile.c', 'bgmaterial.c', 'bgrender.c', 'bghistory.c', 'texencode.c')]
                       + ['-Wl,--gc-sections', '-lm', '-o', str(work / 'edit')], check=True)
        subprocess.run([str(work / 'edit'), str(work)] + native, check=True,
                       env=dict(os.environ, ASAN_OPTIONS='detect_leaks=0', UBSAN_OPTIONS='halt_on_error=1'))


if __name__ == '__main__':
    main()
