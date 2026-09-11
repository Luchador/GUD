#!/usr/bin/env python3
"""Check compiled N64 ammo tables and every setup's command boundaries.

Run after building GUD: python3 tools/geditor/tests/ammo_layout/run.py
Optionally supply current and pre-refactor build directories to compare every
authored ammo quantity, setup command and weapon ammo assignment byte-for-byte.
"""
from collections import Counter
import importlib.util
from pathlib import Path
import re
import struct
import sys


def words(data):
    return list(struct.unpack(f'>{len(data) // 4}I', data))


def commands(data, sizes, end):
    values = words(data)
    at = 0
    result = []
    while at < len(values):
        kind = values[at] & 255
        count = sizes.get(kind, 1)
        assert count > 0 and at + count <= len(values), (kind, at, count)
        result.append(values[at:at + count])
        at += count
        if kind == end:
            assert len(values) - at <= 3 and all(v in (0, 0xffffffff) for v in values[at:]), (at, len(values))
            result[-1].extend(values[at:])
            return result
    raise AssertionError('Missing command terminator')


def main():
    here = Path(__file__).resolve().parent
    root = here.parents[3]
    build = Path(sys.argv[1]) if len(sys.argv) > 1 else root / 'build/u'
    baseline = Path(sys.argv[2]) if len(sys.argv) > 2 else None
    spec = importlib.util.spec_from_file_location('manifest_elf', here.parent / 'manifest_catalogs/run.py')
    elf = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(elf)

    names = re.findall(r'^    (AMMO_\w+),', (root / 'src/ammoconstants.h').read_text(), re.M)
    assert names[:3] == ['AMMO_NONE', 'AMMO_9MM', 'AMMO_RIFLE']
    assert len(names) == 29 and names[12] == 'AMMO_GGUN' and names[-1] == 'AMMO_TOKEN'
    # Use the editor's source-record lengths to walk the actual IDO output.
    loader = (root / 'tools/geditor/src/setupload.c').read_text()
    body = loader.split('static DWORD SetupObjectWordCount(')[1].split('\n}')[0]
    sizes = {}
    for kind, expression in re.findall(r'case (\d+):\s*return ([^;]+);', body):
        expression = expression.replace('AMMOTYPE_GLOBAL_MAX', '12')
        sizes[int(kind)] = sum(int(term.strip(), 0) for term in expression.split('+'))
    assert sizes[20] == 44
    old_sizes = dict(sizes)
    old_sizes[20] = 45
    intro_sizes = dict(enumerate([3, 4, 4, 8, 2, 2, 10, 3, 2, 1]))
    totals = Counter()
    setups = sorted((root / 'assets/obseg/setup').glob('U*.c'))
    for source in setups:
        relative = source.relative_to(root).with_suffix('.o')
        obj = elf.Object(build / relative)
        # This unused multiplayer template still has 63-word door records.
        # It contains no ammo changes; walk its authored layout for comparison.
        prop_sizes = dict(sizes)
        previous_sizes = dict(old_sizes)
        if source.stem == 'Ump_setuprunZ':
            prop_sizes[1] = previous_sizes[1] = 63
        props = commands(obj.bytes('propDefs'), prop_sizes, 48)
        intros = commands(obj.bytes('intro'), intro_sizes, 9)
        for record in props:
            kind = record[0] & 255
            if kind == 20:
                assert len(record) == 44
                totals['crates'] += 1
            elif kind == 7:
                assert 0 <= record[32] < len(names), (source.name, record[32])
                totals['magazines'] += 1
        for record in intros:
            if record[0] & 255 == 2:
                assert 0 <= record[1] < len(names)
                totals['starting ammo records'] += 1
        if baseline:
            old = elf.Object(baseline / relative)
            old_props = commands(old.bytes('propDefs'), previous_sizes, 48)
            old_intros = commands(old.bytes('intro'), intro_sizes, 9)
            for record in old_props:
                kind = record[0] & 255
                if kind == 20:
                    assert record[33] == 0xffff0000, 'Expected unused second 9mm slot'
                    del record[33]
                elif kind == 7:
                    record[32] = 1 if record[32] == 2 else record[32] - (record[32] > 2)
            for record in old_intros:
                if record[0] & 255 == 2:
                    record[1] = 1 if record[1] == 2 else record[1] - (record[1] > 2)
            assert props == old_props, f'Changed unrelated prop data: {source.name}'
            assert intros == old_intros, f'Changed unrelated intro data: {source.name}'

    gun = elf.Object(build / 'src/game/gun.o')
    stats = gun.bytes('ammo_related')
    assert len(stats) == len(names) * 12
    # Reserve capacity and HUD image stay attached to the correct ammo type.
    expected = {1: (800, 0x02000c84), 2: (400, 0x02000c90),
                3: (100, 0x02000c9c), 11: (200, 0x02000ce4),
                12: (100, 0x02000cf0), 27: (50, 0x02000d20)}
    for index, pair in expected.items():
        assert struct.unpack_from('>II', stats, index * 12) == pair, names[index]
    if baseline:
        old_gun = elf.Object(baseline / 'src/game/gun.o')
        old_stats = old_gun.bytes('ammo_related')
        assert stats == old_stats[:24] + old_stats[36:]
        for name in gun.named:
            if not name.endswith('_stats') or name not in old_gun.named:
                continue
            new = words(gun.bytes(name))
            old = words(old_gun.bytes(name))
            # WeaponStats.AmmoType is the eighth word; other data is unchanged.
            old[7] = 1 if old[7] == 2 else old[7] - (old[7] > 2)
            assert new == old, name
    manifest = elf.Object(build / 'src/game/gedmanifest.o')
    assert struct.unpack_from('>I', manifest.bytes('g_GedManifest'), 16)[0] == 3
    print(f'PASS: {len(setups)} N64 setup files, {dict(totals)}, compact ammo IDs, capacities and HUD icons.')
    if baseline:
        print('PASS: all setup commands, pickup/starting quantities and weapon stats preserved after removing the unused slot.')


if __name__ == '__main__':
    main()
