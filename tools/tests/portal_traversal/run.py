#!/usr/bin/env python3
"""Exercise production portal traversal, ring bounds and native room graphs."""
from pathlib import Path
import os
import re
import shlex
import struct
import subprocess
import sys
import tempfile

HERE = Path(__file__).resolve().parent
ROOT = HERE.parents[2]


def function(source, name):
    match = re.search(r'^(?:static )?[\w *]+\b' + name + r'\([^;]*?\)\s*\{', source, re.M)
    assert match, name
    end = source.index('{', match.start()) + 1
    depth = 1
    while depth:
        depth += (source[end] == '{') - (source[end] == '}')
        end += 1
    return source[match.start():end] + '\n'


bg = (ROOT / 'src/game/bg.c').read_text()
header = (ROOT / 'src/game/bg.h').read_text()
source = (HERE / 'harness.h').read_text()
for name in ('BG_PORTAL_QUEUE_LEN', 'BG_PORTAL_SEEN', 'BG_PORTAL_QUEUED'):
    source += re.search(r'^#define ' + name + r'[^\n]*', bg, re.M)[0] + '\n'
source += bg[bg.index('#define BG_PORTAL_CULL_FROM_ROOM1'):bg.index('typedef struct BgPortalBounds')]
source += re.search(r'typedef struct BgPortalBounds \{.*?\} BgPortalBounds;', bg, re.S)[0] + '\n'
source += 'static BgPortalBounds g_BgPortalBounds[PORTMAX];\n'
source += re.search(r'typedef struct s_specialportal\s*\{.*?\} s_specialportal;', header, re.S)[0] + '\n'
source += re.search(r'static const s_specialportal specialportalarray\[\] = \{.*?\n};', bg, re.S)[0] + '\n'
source += re.search(r'typedef struct BgQueuedPortal \{.*?\} BgQueuedPortal;', header, re.S)[0] + '\n'
source += bg[bg.index('BgQueuedPortal g_BgPortalQueue['):bg.index('PortalData *g_BgPortals;', bg.index('BgQueuedPortal g_BgPortalQueue['))]
source += 'static s32 g_BgPortalQueueWriteIndex, g_BgPortalQueueReadIndex;\n'
source += '''
void bgQueuePortalTraversal(s32, s32, s32, s32, f32 *);
void bgProcessPortalTraversal(s32, s32, s32, s32, bbox2d *);
'''
for name in ('bgRectIntersect', 'bgRectOutersect', 'bgGetRoomPortalList', 'bgSetRoomOnScreen',
             'bgResetPortalVisitCounts', 'bgResetPortalQueue', 'bgIncrementRoomPortalVisitCount',
             'bgQueuePortalTraversal', 'bgProcessNextQueuedPortal', 'bgGetRoomCenter',
             'bgCalcPortalPlane', 'bgSwapConnectedRooms', 'bgOrderPortal', 'bgExpandRoomToPortals',
             'bgRoomFitsPortalSide', 'bgBuildPortalCache', 'bgMarkSpecialPortals'):
    source += function(bg, name)
process = function(bg, 'bgProcessPortalTraversal')
process = process.replace('depth + 1, &screenbox)', 'depth + 1, screenbox.f[0])')
source += process.replace('void bgProcessPortalTraversal(', 'static void productionProcessPortalTraversal(')
source += (HERE / 'legacy.c').read_text()
source += '#line 1 "portal_traversal/check.c"\n' + (HERE / 'check.c').read_text()
source += (HERE / 'planes.c').read_text()

with tempfile.TemporaryDirectory(prefix='gud-portal-traversal-') as directory:
    work = Path(directory)
    (work / 'check.c').write_text(source)
    command = shlex.split(os.environ.get('CC', 'cc')) + [
        '-std=c99', '-O1', '-g', '-Wall', '-Wextra', '-Werror',
        '-Wno-sign-compare', '-Wno-missing-braces', '-Wno-return-type', '-fno-strict-aliasing',
        '-fsanitize=address,undefined', str(work / 'check.c'), '-lm', '-o', str(work / 'check')]
    subprocess.run(command, check=True)
    env = dict(os.environ, ASAN_OPTIONS='detect_leaks=0', UBSAN_OPTIONS='halt_on_error=1')
    subprocess.run([str(work / 'check')], check=True, env=env)
    for name in ('azt', 'silo'):
        asset = ROOT / f'assets/obseg/bg/bg_{name}_all_p.c'
        pairs = re.findall(r'\{&portal_\d+,\s*0x([0-9A-Fa-f]+),\s*0x([0-9A-Fa-f]+),', asset.read_text())
        fixture = work / (name + '.txt')
        fixture.write_text('\n'.join(f'{int(a,16)} {int(b,16)}' for a,b in pairs))
        subprocess.run([str(work / 'check'), str(fixture)], check=True, env=env)

    # Optional uncompressed GEditor BG: use its exact vertices, origins and
    # portal polygons, with a controlled visible/off-screen projection.
    for path in sys.argv[1:]:
        data = Path(path).read_bytes()
        word = lambda offset: struct.unpack_from('>I', data, offset)[0]
        assert word(0) == 0
        table, portals = word(4) & 0xffffff, word(8) & 0xffffff
        lines = []
        for room in range(1, 139):
            row = table + room * 24
            if not word(row + 4):
                break
            start = word(row) & 0xffffff
            count = word(start - 4) // 16 if start else 0
            origin = struct.unpack_from('>3f', data, row + 12)
            points = [struct.unpack_from('>3h', data, start + v * 16) for v in range(count)]
            lo = [origin[a] + min(p[a] for p in points) for a in range(3)] if points else origin
            hi = [origin[a] + max(p[a] for p in points) for a in range(3)] if points else origin
            lines.append(' '.join(map(str, [room, *lo, *hi])))
        lines.insert(0, str(len(lines)))
        entries = []
        for index in range(200):
            row = portals + index * 8
            start = word(row) & 0xffffff
            if not start:
                break
            count = data[start]
            assert 3 <= count <= 8
            values = [*data[row + 4:row + 8], count]
            for point in range(count):
                values.extend(struct.unpack_from('>3f', data, start + 4 + point * 12))
            entries.append(' '.join(map(str, values)))
        lines += [str(len(entries)), *entries]
        fixture = work / 'geometry.txt'
        fixture.write_text('\n'.join(lines))
        subprocess.run([str(work / 'check'), '--geometry', str(fixture)], check=True, env=env)
