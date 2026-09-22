#!/usr/bin/env python3
"""Exercise production occlusion, both render passes, attachment effects and RNG."""
from pathlib import Path
import os
import re
import struct
import zlib
import math
import subprocess
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


# The runtime uses whole-model radii at each posed joint, deliberately avoiding
# unproven smaller per-limb bounds. Verify the local-space premise on real assets.
assets = 0
for kind in ('chr', 'prop'):
    for path in sorted((ROOT / 'assets/obseg' / kind).glob('*Z.bin')):
        header = path.parent / path.stem[1:-1] / ('modelFileHeader.inc.c' if kind == 'chr' else 'ModelFileHeader.inc.c')
        if not header.exists():
            continue
        args = re.search(r'MODELFILEHEADER\((.*)\)', header.read_text())[1].split(',')
        radius = float(args[6])
        blob = path.read_bytes()
        if blob[:2] == b'\x11\x72':
            blob = zlib.decompress(blob[2:], -15)
        word = lambda o: struct.unpack_from('>I', blob, o)[0]
        half = lambda o: struct.unpack_from('>H', blob, o)[0]
        ptr = lambda o: word(o) & 0xffffff
        pending = [int(args[4], 0) * 4 + int(args[8], 0) * 12]
        seen = set()
        while pending:
            node = pending.pop()
            if node in seen:
                continue
            seen.add(node)
            for offset in (12, 20):
                if word(node + offset):
                    pending.append(ptr(node + offset))
            opcode, ro = half(node) & 255, ptr(node + 4)
            if opcode not in (4, 24):
                continue
            vertices = ptr(ro + (12 if opcode == 4 else 8))
            count = half(ro + (16 if opcode == 4 else 12))
            for i in range(count):
                xyz = struct.unpack_from('>hhh', blob, vertices + i * 16)
                assert math.sqrt(sum(x*x for x in xyz)) <= radius + 1, path.name
        assets += 1
assert assets >= 420
print(f'{assets} stock body/head/prop assets: local vertices fit full-model bounds', flush=True)

constants = (ROOT / 'src/bondconstants.h').read_text()
model = (ROOT / 'src/game/model.c').read_text()
chr = (ROOT / 'src/game/chr.c').read_text()
source = re.search(r'typedef enum MODELNODE_OPCODE\s*\{.*?\}\s*\w+;', constants, re.S)[0] + '\n'
for token in ('CHRFLAG_NOFADE', 'CHRFLAG_00020000', 'CHRFLAG_CULL_USING_HITBOX', 'CHRFLAG_NO_SHADOW'):
    value = re.search(r'\b' + token + r'\s*=\s*(0x[0-9a-fA-F]+|\d+)', constants)
    source += f'#define {token} {value[1]}\n'
source += '\n'.join(re.findall(r'^#define MODEL_RENDER_.*', (ROOT / 'src/game/model.h').read_text(), re.M)) + '\n'
source += (HERE / 'harness.h').read_text()
for name in ('modelFindNodeMtxIndex', 'getjointsize'):
    source += function(model, name)
source += re.search(r'static const s16 g_GunfireUvOffsets32.*?\n};', model, re.S)[0] + '\n'
for name in ('modelRenderGunfire', 'modelRenderRotatingTexture'):
    source += function(model, name)
for name in ('modelHitRenderNodeListFiltered', 'modelHitRenderNodeList'):
    source += function((ROOT / 'src/game/objecthandler.c').read_text(), name)
source += function((ROOT / 'src/game/explosion.c').read_text(), 'explosionRenderBulletImpactOnPropFiltered')
occlusion = (ROOT / 'src/game/occlusion.c').read_text()
source += occlusion[occlusion.index('static OcclusionBox *g_Occluders;'):occlusion.index('void occlusionReset')]
source += function(occlusion, 'occlusionTestCharacter')
source += re.search(r'^#define CHR_ATTACHMENT_RENDER_DISTANCE .*$', chr, re.M)[0] + '\n'
for name in ('chrHideDistantAttachments', 'chrGetHeadSwitch', 'chrGetHatCoveredHeadNode', 'chrRenderChr'):
    source += function(chr, name)
source += (HERE / 'check.c').read_text()
source += (HERE / 'attachments.c').read_text()
assert 'occlusion' not in function(chr, 'chrTick')
with tempfile.TemporaryDirectory(prefix='gud-character-occlusion-') as directory:
    work = Path(directory)
    (work / 'check.c').write_text(source)
    subprocess.run([os.environ.get('CC', 'cc'), '-std=c99', '-O1', '-g', '-Wall', '-Wextra',
                    '-Werror', '-Wno-unused-variable', '-Wno-unused-but-set-variable',
                    '-Wno-missing-braces', '-ffp-contract=off', '-fsanitize=address,undefined',
                    '-fno-sanitize=shift', f'-I{ROOT}', str(work / 'check.c'),
                    str(ROOT / 'src/game/occlusionmath.c'), '-lm', '-o', str(work / 'check')], check=True)
    subprocess.run([str(work / 'check')], check=True,
                   env=dict(os.environ, ASAN_OPTIONS='detect_leaks=0', UBSAN_OPTIONS='halt_on_error=1'))
