#!/usr/bin/env python3
"""Face clipboard snapshots, paste/history, native save/reload and scoped keys."""
import importlib.util
import os
from pathlib import Path
import re
import struct
import subprocess
import sys
import tempfile


def load(name, path):
    spec = importlib.util.spec_from_file_location(name, path)
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


def main():
    sys.dont_write_bytecode = True
    here = Path(__file__).resolve().parent
    src = here.parents[1] / 'src'
    helpers = load('extract', here.parent / 'portal_editing/run.py')
    native = load('native', here.parent / 'bg_transparency/run.py')
    with tempfile.TemporaryDirectory(prefix='geditor-face-clipboard-') as temp:
        work = Path(temp)
        fixture = (here.parent / 'bg_transparency/edit.c').read_text()
        (work / 'fixture.inc').write_text(''.join(helpers.function(fixture, n) for n in
            ('Put', 'Fixture', 'Refs', 'RoundTrip')))
        common = (here.parent / 'bg_disconnect/check.c').read_text()
        (work / 'common.inc').write_text(''.join(helpers.function(common, n) for n in ('Same', 'UseCounts')))
        editor = (src / 'geditor.c').read_text()
        (work / 'editor.inc').write_text(''.join(helpers.function(editor, n) for n in
            ('GEditorCanFlipSelectedBgFaces', 'GEditorCanPasteBgFaces',
             'GEditorCopySelectedBgFaces', 'GEditorPasteBgFaceSnapshot', 'GEditorPasteBgFaces', 'GEditorDuplicateBgFaces')))
        paths = []
        for name in ('depo', 'run'):
            source = (here.parents[3] / f'assets/obseg/bg/bg_{name}_all_p.c').read_text()
            data = native.jungle_fixture(source)
            for room, x, y, z in re.findall(r'\{&point_table_binary_(\d+),[^\n]+?, (-?[\d.]+), (-?[\d.]+), (-?[\d.]+)\}', source):
                struct.pack_into('>fff', data, 32 + int(room)*24 + 12, float(x), float(y), float(z))
            path = work / f'{name}.seg'; path.write_bytes(data); paths.append(str(path))
        command = [os.environ.get('CC', 'cc'), '-std=c99', '-O1', '-g', '-Wall', '-Wextra', '-Werror',
                   '-Wno-unused-parameter', '-ffunction-sections', '-fdata-sections', '-fsanitize=address,undefined',
                   f'-I{here.parent / "image_import"}', f'-I{src}', f'-I{work}']
        sources = ('bgload.c', 'bgcompile.c', 'bgmaterial.c', 'bgrender.c', 'bghistory.c')
        subprocess.run(command + [str(here / 'check.c'), str(here.parent / 'image_import/platform.c')]
                       + [str(src / n) for n in sources] + ['-Wl,--gc-sections', '-lm', '-o', str(work / 'check')], check=True)
        env = dict(os.environ, ASAN_OPTIONS='detect_leaks=0', UBSAN_OPTIONS='halt_on_error=1')
        subprocess.run([str(work / 'check'), str(work)] + paths, check=True, env=env)
        (work / 'input.inc').write_text(helpers.function(editor, 'GEditorHandleFaceClipboardHotkey'))
        subprocess.run(command + [str(here / 'input.c'), str(here.parent / 'image_import/platform.c'),
                                 '-Wl,--gc-sections', '-o', str(work / 'input')], check=True)
        subprocess.run([str(work / 'input')], check=True, env=env)


if __name__ == '__main__':
    main()
