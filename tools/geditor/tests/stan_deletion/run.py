#!/usr/bin/env python3
"""Native stan deletion, viewport selection, history and ROM-resource regression checks."""
import importlib.util
import os
from pathlib import Path
import re
import struct
import subprocess
import tempfile

here = Path(__file__).resolve().parent
src = here.parents[1] / 'src'
root = src.parents[2]
spec = importlib.util.spec_from_file_location('extract', here.parent / 'portal_editing/run.py')
extract = importlib.util.module_from_spec(spec)
spec.loader.exec_module(extract)
with tempfile.TemporaryDirectory(prefix='geditor-stan-delete-') as temp:
    work = Path(temp)
    (work / 'stan').mkdir()
    # Pack the repository's actual Depot tile records, without a copyrighted ROM.
    source = (root / 'assets/obseg/stan/Tbg_depo_all_p_stanZ.c').read_text()
    depot = bytearray(struct.pack('>III', 0, 0x0e00000c, 0))
    for body in re.findall(r'StandTile tile_\d+ = \{(.*?)\n\};', source, re.S):
        values = [int(n, 0) for n in re.findall(r'-?0x[0-9a-fA-F]+|-?\d+', body)]
        ident, room, special, red, green, blue, count, a, b, c = values[:10]
        assert len(values) == 10 + count * 4
        depot += struct.pack('>IHH', (ident << 8) | room, (special << 12) | (red << 8) | (green << 4) | blue,
                             (count << 12) | (a << 8) | (b << 4) | c)
        for p in range(count):
            depot += struct.pack('>hhhH', *values[10 + p * 4:14 + p * 4])
    depot += b'unstric\0' + bytes(16)
    (work / 'stan/Tbg_depo_all_p_stanZ.stan').write_bytes(depot)
    viewport = (src / 'viewport.c').read_text()
    (work / 'selection.inc').write_text(''.join(extract.function(viewport, n) for n in
        ('ViewportStanVisible', 'ViewportGetStanSelectionCount', 'ViewportGetSelectedStanTiles')))
    (work / 'controller.inc').write_text(extract.function((src / 'geditor.c').read_text(), 'GEditorDeleteSelectedStanTiles'))
    export = (src / 'romexport.c').read_text()
    (work / 'export.inc').write_text(''.join(extract.function(export, n) for n in
        ('RomExportSetError', 'RomExportEndsWith', 'RomExportSimpleResourceName', 'RomExportProjectResourcePath', 'RomExportReadResource')))
    binary = work / 'check'
    subprocess.run([os.environ.get('CC', 'cc'), '-std=c99', '-O1', '-g', '-Wall', '-Wextra', '-Werror',
                    '-Wno-unused-parameter', '-ffunction-sections', '-fdata-sections', '-fsanitize=address,undefined',
                    f'-I{here.parent / "image_import"}', f'-I{src}', f'-I{work}', str(here / 'check.c'),
                    str(here.parent / 'image_import/platform.c'),
                    *[str(src / name) for name in ('stanload.c', 'standelete.c', 'stanquery.c', 'bghistory.c')],
                    '-Wl,--gc-sections', '-Wl,--wrap=malloc', '-Wl,--wrap=calloc', '-lm', '-o', str(binary)], check=True)
    subprocess.run([str(binary), str(work)], check=True,
                   env=dict(os.environ, ASAN_OPTIONS='detect_leaks=0', UBSAN_OPTIONS='halt_on_error=1'))
