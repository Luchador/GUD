#!/usr/bin/env python3
"""Check allocation-slot pointers and manifest spans in real IDO MIPS objects."""
import importlib.util
from pathlib import Path
import struct
import sys

here = Path(__file__).resolve().parent
root = here.parents[3]
build = Path(sys.argv[1]) if len(sys.argv) > 1 else root / 'build/u'
sys.dont_write_bytecode = True
spec = importlib.util.spec_from_file_location('objects', here.parent / 'manifest_catalogs/run.py')
objects = importlib.util.module_from_spec(spec)
spec.loader.exec_module(objects)
manifest = objects.Object(build / 'src/game/gedmanifest.o')
header = manifest.bytes('g_GedManifest', 24)
version, count = struct.unpack_from('>II', header, 16)
assert version == 3 and count <= 32
data = manifest.bytes('g_GedManifest', 24 + count * 16)
entries = {data[24+i*16:28+i*16].decode(): 24+i*16 for i in range(count)}
assert len(entries) == count
stage = entries['STGT']
levels = struct.unpack_from('>I', data, stage + 12)[0]
assert levels == 36
memory = entries['LMEM']
assert struct.unpack_from('>I', data, memory + 12)[0] == 64
manifest.pointer('g_GedManifest', memory + 4, '_gedLevelMemoryRom')
manifest.pointer('g_GedManifest', memory + 8, '_gedLevelMemoryRom', levels * 64)
lv = objects.Object(build / 'src/game/lv.o')
# IDO symbol sizes for initialized nested aggregates are incomplete; the
# manifest's sizeof-derived spans cover the full emitted bytes.
rows = lv.bytes('g_LevelInfoTable', levels * 40)
slots = lv.bytes('g_LevelMemoryAllocationStrings', levels * 64)
expected = {30: (60, 50, 710, 300), 90: (80, 20, 646, 1),
            27: (100, 50, 725, 150), 427: (130, 100, 550, 170)}
for i in range(levels):
    level = struct.unpack_from('>I', rows, i * 40)[0]
    if level == 57:
        assert struct.unpack_from('>I', rows, i * 40 + 20)[0] == 0
        continue
    lv.pointer('g_LevelInfoTable', i * 40 + 20, 'g_LevelMemoryAllocationStrings', i * 64)
    slot = slots[i*64:(i+1)*64]
    assert 0 in slot
    tokens = slot.split(b'\0', 1)[0].decode().split()
    assert tokens[:2] == ['-ml0', '-me0'] and len(tokens) == 6
    values = tuple(int(token[len(key):], 10) for token, key in zip(tokens[2:], ('-mgfx', '-mvtx', '-mt', '-ma')))
    if level in expected:
        assert values == expected[level], (level, values)
assert '_gedLevelMemoryRom = _csegmentSegmentRomStart + (g_LevelMemoryAllocationStrings - _csegmentSegmentStart)' in (root / 'ge007.ld').read_text()
print('PASS: native STGT layout, 35 independent allocation slots, unchanged Depot/solo/MP/Title budgets and bounded LMEM manifest relocations.')
