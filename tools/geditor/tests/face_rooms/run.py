#!/usr/bin/env python3
"""Room transfers, native save/reload (including Depot), and editor history."""
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
    with tempfile.TemporaryDirectory(prefix='geditor-face-rooms-') as temp:
        work = Path(temp)
        fixture = (here.parent / 'bg_transparency/edit.c').read_text()
        (work / 'fixture.inc').write_text(''.join(helpers.function(fixture, n) for n in ('Put', 'Fixture', 'Refs')))
        common = (here.parent / 'bg_disconnect/check.c').read_text()
        (work / 'common.inc').write_text(''.join(helpers.function(common, n) for n in ('Same', 'UseCounts')))
        # Round trips may batch opaque triangles into a different order.
        batching = (here.parent / 'texture_batching/check.c').read_text()
        comparison = re.search(r'typedef struct FaceKey\s*\{.*?\} FaceKey;', batching, re.S)[0]
        comparison += '\n' + ''.join(helpers.function(batching, name) for name in
                                     ('Key', 'Compare', 'EquivalentDocuments'))
        (work / 'comparison.inc').write_text(comparison.replace('OK(', 'assert('))
        (work / 'editor.inc').write_text(helpers.function((src / 'geditor.c').read_text(), 'GEditorMoveSelectedFacesToRoom'))
        depot = (here.parents[3] / 'assets/obseg/bg/bg_depo_all_p.c').read_text()
        data = native.jungle_fixture(depot)
        records = re.findall(r'\{&point_table_binary_(\d+),[^\n]+?, (-?[\d.]+), (-?[\d.]+), (-?[\d.]+)\}', depot)
        assert len(records) == 90
        for room, x, y, z in records:
            struct.pack_into('>fff', data, 32 + int(room) * 24 + 12, float(x), float(y), float(z))
        (work / 'depot.seg').write_bytes(data)
        command = [os.environ.get('CC', 'cc'), '-std=c99', '-O1', '-g', '-Wall', '-Wextra', '-Werror',
                   '-Wno-unused-parameter', '-ffunction-sections', '-fdata-sections', '-fsanitize=address,undefined',
                   f'-I{here.parent / "image_import"}', f'-I{src}', f'-I{work}']
        sources = ('bgload.c', 'bgcompile.c', 'bgmaterial.c', 'bgrender.c', 'bghistory.c')
        subprocess.run(command + [str(here / 'check.c'), str(here.parent / 'image_import/platform.c')]
                       + [str(src / n) for n in sources] + ['-Wl,--gc-sections', '-lm', '-o', str(work / 'check')], check=True)
        subprocess.run([str(work / 'check'), str(work), str(work / 'depot.seg')], check=True,
                       env=dict(os.environ, ASAN_OPTIONS='detect_leaks=0', UBSAN_OPTIONS='halt_on_error=1'))

        inspector = (src / 'faceproperties.c').read_text()
        (work / 'input.inc').write_text(''.join(helpers.function(inspector, n) for n in
            ('FacePropertiesApplyRoom', 'FacePropertiesHandleMessage'))
            + helpers.function((src / 'geditor.c').read_text(), 'GEditorCreateAccelerators'))
        subprocess.run(command + [str(here / 'input.c'), '-Wl,--gc-sections', '-o', str(work / 'input')], check=True)
        subprocess.run([str(work / 'input')], check=True,
                       env=dict(os.environ, ASAN_OPTIONS='detect_leaks=0', UBSAN_OPTIONS='halt_on_error=1'))


if __name__ == '__main__':
    main()
