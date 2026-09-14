#!/usr/bin/env python3
"""Check production hat transforms, cache refreshes and hit-list construction."""
from pathlib import Path
import os
import re
import shlex
import struct
import subprocess
import tempfile
import zlib

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


constants = (ROOT / 'src/bondconstants.h').read_text()
types = (ROOT / 'src/bondtypes.h').read_text()
chr = (ROOT / 'src/game/chr.c').read_text()
props = (ROOT / 'src/game/propobj.c').read_text()
model = (ROOT / 'src/game/model.c').read_text()
hits = (ROOT / 'src/game/objecthandler.c').read_text()
matrix = (ROOT / 'src/game/matrixmath.c').read_text()
source = ''
for name in ('MODELNODE_OPCODE', 'HATTYPE', 'GUNHAND', 'BODIES', 'PROP'):
    source += re.search(r'typedef enum ' + name + r'\s*\{.*?\}\s*\w+;', constants, re.S)[0] + '\n'
source += (HERE / 'harness.h').read_text()
for name in ('ModelHitEntry', 'ModelHitList', 'ChrHatCache'):
    source += re.search(r'typedef struct ' + name + r'\s*\{.*?\}\s*' + name + ';', types, re.S)[0] + '\n'
source += '''
typedef struct ChrRecord {
    Model *model; PropRecord *handle_positiondata_hat; s8 headnum; u16 hidden;
    PropRecord *weapons_held[2];
    ChrHatCache hatcache;
} ChrRecord;
static ModelHitEntry entries[80], *g_ModelHitFreeList;
static ModelRenderData D_800322A4 = {0};
static PropRecord *chrGetEquippedWeaponProp(ChrRecord *chr, GUNHAND hand) { return chr->weapons_held[hand]; }
#define M_PI_F 3.14159265358979323846f
'''
for token in ('CHRHIDDEN_DROP_HELD_ITEMS', 'PROPRUNTIMEFLAG_ONSCREEN', 'RUNTIMEBITFLAG_HASPROJECTILE',
              'RUNTIMEBITFLAG_00000800', 'PROPFLAG2_ONLYEXPLOSIONDAMAGE'):
    catalog = constants + (ROOT / 'src/propruntimeflags.h').read_text() + (ROOT / 'src/propconstants.h').read_text()
    value = re.search(r'\b' + token + r'\s*=\s*(0x[0-9a-fA-F]+|\d+)', catalog)
    assert value, token
    source += f'#define {token} {value[1]}\n'
source += re.search(r'struct headHat\s*\{.*?\};', (ROOT / 'src/game/chrobjdata.h').read_text(), re.S)[0] + '\n'
source += (ROOT / 'assets/obseg/chr/chrHeadHats.inc.c').read_text()
source += function(props, 'get_hat_model').replace('ObjectRecord *objinst', 'typeLookups++;\n    ObjectRecord *objinst')
for name in ('modelHitAppendModel', 'modelHitAppendNode', 'modelHitBuildNodeList', 'modelHitFreeChain', 'modelHitSortByDepth'):
    source += function(hits, name)
source += (HERE / 'reference.c').read_text()
for name in ('matrix_4x4_set_identity_and_position', 'matrix_4x4_multiply_translation', 'matrix_4x4_apply_scale_and_translation',
             'matrix_4x4_set_rotation_around_z', 'matrix_4x4_multiply', 'matrix_4x4_copy', 'matrix_4x4_multiply_in_place'):
    source += function(matrix, name)
for name in ('modelFindNodeMtxIndex', 'modelFindNodeMtx', 'process_15_subposition', 'instcalcmatrices'):
    f = function(model, name).replace('&matrices[mtxindex]);', '&matrices[mtxindex].pos);')
    if name == 'modelFindNodeMtxIndex':
        f = f.replace('s32 index;', 's32 index;\n    matrixLookups++;')
    source += f
for name in ('chrHatHasSimpleModel', 'chrRefreshHatCache', 'chrBuildHatMatrix', 'chrUpdateHat'):
    source += function(chr, name)
source += function(props, 'chrRenderHeldWeapon')

# Decode every real hat hierarchy and origin; do not assume all imported models
# are simple merely because they use a familiar filename or hat ID.
fixtures = []
for path in sorted((ROOT / 'assets/obseg/prop').glob('Phat*Z.bin')):
    name = path.stem[1:-1]
    header = ROOT / f'assets/obseg/prop/{name}/ModelFileHeader.inc.c'
    if not header.exists() or '&SKELETON(hat)' not in header.read_text():
        continue
    args = re.search(r'MODELFILEHEADER\((.*)\)', header.read_text())[1].split(',')
    b = path.read_bytes()
    if b[:2] == b'\x11\x72':
        b = zlib.decompress(b[2:], -15)
    word = lambda off: struct.unpack_from('>I', b, off)[0]
    root = int(args[4], 0) * 4 + int(args[8], 0) * 12
    nodes, node = [], root
    while node:
        op = struct.unpack_from('>H', b, node)[0]
        assert word(node + 12) == word(node + 16) == 0
        assert word(node + 8) & 0xffffff == (nodes[-1][0] if nodes else 0)
        nodes.append((node, op))
        node = word(node + 20) & 0xffffff
    assert [op for _, op in nodes] == [0x15, 0xa, 4]
    assert int(args[5]) == 1
    ro = word(root + 4) & 0xffffff
    assert struct.unpack_from('>h', b, ro + 12)[0] == 0
    xyz = struct.unpack_from('>fff', b, ro)
    fixtures.append('{ PROP_' + name.upper() + ', {{' + ','.join(float(v).hex() + 'f' for v in xyz) + '}} }')
assert len(fixtures) == 12
source += 'static const struct { int id; coord3d origin; } assets[] = {\n' + ',\n'.join(fixtures) + '\n};\n'
source += (HERE / 'check.c').read_text()

# Verify lifetime hooks and typed builder integration in the real callers.
assert 'chr->hatcache.model = NULL;' in function(chr, 'init_GUARDdata_with_set_values')
assert 'chr->hatcache.model = NULL;' in function(props, 'hatApplyToChr')
assert 'chr->hatcache.model = NULL;' in function(props, 'objDetach')
tick = function(chr, 'chrTick')
assert 'chr->hitChain = hitlist.head;' in tick and '(Gfx **)(&chr->hitChain)' not in tick
assert tick.index('chrUpdateHat(') < tick.index('modelHitCalculateNodeDepths(') < tick.index('objDrop(')
assert 'modelHitAppendModel(hitlist, heldModel);' in function(props, 'chrRenderHeldWeapon')

with tempfile.TemporaryDirectory(prefix='gud-hat-optimizations-') as directory:
    work = Path(directory)
    (work / 'check.c').write_text(source)
    command = shlex.split(os.environ.get('CC', 'cc'))
    command += ['-std=c99', '-O2', '-Wall', '-Wextra', '-Werror', '-Wno-unused-variable',
                '-Wno-missing-braces', '-Wno-sign-compare', '-ffp-contract=off', '-fno-strict-aliasing']
    command += shlex.split(os.environ.get('TEST_CFLAGS', ''))
    command += [str(work / 'check.c'), '-lm', '-o', str(work / 'check')]
    subprocess.run(command, check=True)
    subprocess.run([str(work / 'check')], check=True)
