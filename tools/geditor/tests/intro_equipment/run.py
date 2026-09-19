#!/usr/bin/env python3
"""Starting equipment: native edits, unrelated resources, persistence and undo."""
import importlib.util
import os
from pathlib import Path
import struct
import subprocess
import tempfile

here = Path(__file__).resolve().parent
src = here.parents[1] / 'src'
root = src.parents[2]

def load(name, path):
    spec = importlib.util.spec_from_file_location(name, path)
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module

compact = load('compact', here.parent / 'setup_compaction/run.py')
extract = load('extract', here.parent / 'portal_editing/run.py')
source = bytearray(compact.fixture())
intro = [0,0,0, 1,4,-1,0, 2,1,100,0, 5,0, 1,5,5,2, 2,1,9,2,
         6,100,200,300,0,0,0,0,0,0, 1,8,-1,0, 2,2,50,0,
         8,0x840, 7,7,30, 3,1,0,0,0,0,0,0, 9]
struct.pack_into('>I', source, 8, 0x900)
struct.pack_into('>' + 'I' * len(intro), source, 0x900, *(v & 0xffffffff for v in intro))
with tempfile.TemporaryDirectory(prefix='geditor-intro-equipment-') as directory:
    work = Path(directory)
    (work / 'setup').mkdir()
    (work / 'setup/UsetupintroZ.set').write_bytes(source)
    (work / 'quantity.inc').write_text(extract.function((src/'levelmanager.c').read_text(), 'LevelManagerParseQuantity'))
    (work / 'export.inc').write_text(''.join(extract.function((src/'romexport.c').read_text(), name)
        for name in ('RomExportSetError','RomExportReadResource')))
    command = [os.environ.get('CC','cc'), '-std=c99', '-O1', '-g', '-Wall', '-Wextra', '-Werror',
               '-Wno-unused-parameter', '-ffunction-sections', '-fdata-sections', '-fsanitize=address,undefined',
               f'-I{here.parent/"image_import"}', f'-I{src}', f'-I{root}', f'-I{work}',
               str(here/'check.c'), str(here.parent/'image_import/platform.c')]
    command += [str(src/name) for name in ('setupload.c','actionblocks.c','bghistory.c')]
    command += ['-Wl,--gc-sections','-Wl,--wrap=malloc','-Wl,--wrap=calloc','-Wl,--wrap=realloc','-lm','-o',str(work/'check')]
    subprocess.run(command, check=True)
    subprocess.run([str(work/'check'),str(work)], check=True,
                   env=dict(os.environ, ASAN_OPTIONS='detect_leaks=0', UBSAN_OPTIONS='halt_on_error=1'))
    # Independent native reader checks every live table, including credits pointers.
    before = compact.signature(source)
    after = compact.signature((work/'setup/Usetupintro_finalZ.set').read_bytes())
    def without_equipment(records):
        return [r for r in records if not (isinstance(r, (bytes, bytearray))
                and struct.unpack_from('>I',r)[0] in (1,2) and struct.unpack_from('>I',r,12)[0]==0)]
    assert without_equipment(before[2]) == without_equipment(after[2])
    assert before[:2] + before[3:] == after[:2] + after[3:]
    print('PASS: cameras, spawn, swirl, watch, credits, demo equipment, pads, props, scripts and native references remain intact.')
