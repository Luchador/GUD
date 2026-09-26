#!/usr/bin/env python3
"""Group transforms use real native placement, serialization and history."""
import os
from pathlib import Path
import runpy
import struct
import subprocess
import tempfile

here = Path(__file__).resolve().parent
src = here.parents[1] / 'src'
shim = here.parent / 'image_import'
data = runpy.run_path(str(here.parent / 'door_placement/run.py'))['fixture']()
start = (len(data) + 3) & ~3
data += bytes(start - len(data))
for kind in (3, 3, 1, 3):
    record = bytearray((64 if kind == 1 else 32) * 4)
    struct.pack_into('>4I', record, 0, 0x01000000 | kind, 1 << 16, 0, 0x100)
    struct.pack_into('>I', record, 0x74, 777 << 16)
    data += record
data += struct.pack('>I', 48)
struct.pack_into('>I', data, 12, start)
with tempfile.TemporaryDirectory(prefix='geditor-object-group-') as temp:
    work = Path(temp)
    (work / 'setup').mkdir()
    (work / 'setup/UsetupgroupZ.set').write_bytes(data)
    extract = runpy.run_path(str(here.parent / 'object_properties/run.py'))['function']
    character = (src / 'characterload.c').read_text()
    (work / 'character-placement.inc').write_text(''.join(extract(character, name) for name in
        ('CharacterGetPadPlacement', 'CharacterGetPadPosition')))
    (work / 'culling.inc').write_text(extract((src / 'bgrender.c').read_text(), 'BgRenderResolveModelCulling'))
    assets = (here.parent / 'object_duplicate/assets.c').read_text().replace(
        'BgVertex *ModelLoadProjectGeometry(', 'static BgVertex *FixtureModelLoadProjectGeometry(', 1)
    (work / 'assets.c').write_text(assets + '''
int groupFailAfter = -1;
BgVertex *ModelLoadProjectGeometry(const char *dir, int id, DWORD *count, unsigned short **tags,
    BgRenderFlags **flags, float *scale, const char **why)
{
    if (groupFailAfter == 0) { return NULL; }
    if (groupFailAfter > 0) { groupFailAfter--; }
    return FixtureModelLoadProjectGeometry(dir, id, count, tags, flags, scale, why);
}
''')
    sources = ('setupload.c', 'actionblocks.c', 'bghistory.c', 'objectload.c', 'objecttransform.c',
               'setupselection.c', 'objectshade.c', 'rotation.c', 'scaling.c', 'stanquery.c')
    subprocess.run([os.environ.get('CC', 'cc'), '-O1', '-g', '-std=c99', '-Wall', '-Wextra',
        '-Werror', '-Wno-unused-parameter', '-ffunction-sections', '-fdata-sections',
        '-fsanitize=address,undefined', f'-I{shim}', f'-I{src}', f'-I{src.parents[2]}', f'-I{work}',
        str(here / 'check.c'), str(work / 'assets.c'), str(shim / 'platform.c'),
        *[str(src / name) for name in sources], '-Wl,--gc-sections', '-lm', '-o', str(work / 'check')], check=True)
    subprocess.run([str(work / 'check'), str(work)], check=True,
        env=dict(os.environ, ASAN_OPTIONS='detect_leaks=0', UBSAN_OPTIONS='halt_on_error=1'))
