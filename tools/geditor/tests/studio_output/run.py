#!/usr/bin/env python3
"""Exercise the production BMP writer and GUD alpha importer with a Win32 file shim."""
import importlib.util
import os
from pathlib import Path
import struct
import subprocess
import tempfile

here = Path(__file__).resolve().parent
src = here.parent.parent / 'src'
shim = here.parent / 'project_rebase'
spec = importlib.util.spec_from_file_location('image_tests', here.parent / 'image_import/run.py')
image_tests = importlib.util.module_from_spec(spec)
spec.loader.exec_module(image_tests)
source = (src / 'texload.c').read_text()
with tempfile.TemporaryDirectory(prefix='geditor-studio-output-') as temp:
    work = Path(temp)
    native = '#include <stdlib.h>\n#include "texload.h"\n'
    for name in ('texle16', 'texle32', 'TexRestoreImportBmpAlpha'):
        native += image_tests.function(source, name) + '\n'
    native += 'BOOL TestRestoreImportBmpAlpha(const char *p,TexPixel *s,DWORD w,DWORD h) { return TexRestoreImportBmpAlpha(p,s,w,h); }\n'
    (work / 'alpha.c').write_text(native)
    command = [os.environ.get('CC', 'cc'), '-O1', '-g', '-std=c99', '-Wall', '-Wextra', '-Werror',
               '-Wno-unused-parameter', '-Wno-format', '-ffunction-sections', '-fdata-sections',
               '-fsanitize=address,undefined', '-Dfopen=TestFopen', f'-I{shim}', f'-I{src}',
               f'-I{src.parents[2]}', str(here / 'check.c'), str(shim / 'platform.c'), str(work / 'alpha.c'),
               str(src / 'studiooutput.c'), str(src / 'project.c'), '-Wl,--gc-sections', '-o', str(work / 'check')]
    subprocess.run(command, check=True)
    subprocess.run([str(work / 'check'), str(work)], check=True,
                   env=dict(os.environ, ASAN_OPTIONS='detect_leaks=0', UBSAN_OPTIONS='halt_on_error=1'))
    output = work / 'studio/output'
    assert {p.name for p in output.iterdir()} == {f'render{i:04d}.bmp' for i in range(1, 7)}
    assert (output / 'render0003.bmp').is_dir()
    expected = bytes([0, 0, 255, 255, 0, 0, 0, 255, 0, 255, 0, 255,
                      255, 0, 0, 255, 0, 0, 0, 0, 255, 255, 255, 255])
    for index in (1, 2, 4):
        data = (output / f'render{index:04d}.bmp').read_bytes()
        assert data[:2] == b'BM' and len(data) == 146
        assert struct.unpack_from('<I', data, 2)[0] == len(data)
        assert struct.unpack_from('<IIIIHHII', data, 10) == (122, 108, 3, 2, 1, 32, 3, 24)
        assert struct.unpack_from('<IIIII', data, 54) == (0xff0000, 0xff00, 0xff, 0xff000000, 0x73524742)
        assert data[122:] == expected
    assert not any((output / 'render0005.bmp').read_bytes()[122:])
    assert (output / 'render0006.bmp').read_bytes()[122:] == b'\0\0\0\xff'
    print('PASS: independently decoded V4 headers, dimensions, RGB channel order, bottom-up orientation, explicit alpha and no orphan temporary files.')
