#!/usr/bin/env python3
"""Check production LOD dependency analysis and joint builders against real assets."""
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


def fixtures():
    output, assets, commands = [], [], []
    for path in sorted((ROOT / 'assets/obseg/chr').glob('C*Z.bin')):
        header = path.parent / path.stem[1:-1] / 'modelFileHeader.inc.c'
        if not header.exists():
            continue
        args = re.search(r'MODELFILEHEADER\((.*)\)', header.read_text())[1].split(',')
        data = path.read_bytes()
        if data[:2] == b'\x11\x72':
            data = zlib.decompress(data[2:], -15)
        word = lambda o: struct.unpack_from('>I', data, o)[0]
        half = lambda o: struct.unpack_from('>H', data, o)[0]
        ptr = lambda o: word(o) & 0xffffff
        root = int(args[4], 0) * 4 + int(args[8], 0) * 12
        pending, nodes = [root], []
        while pending:
            node = pending.pop()
            if node in nodes:
                continue
            nodes.append(node)
            for off in (12, 20):
                if word(node + off):
                    pending.append(ptr(node + off))
        assert len(nodes) <= 256
        indexes = {n: i for i, n in enumerate(nodes)}
        index = lambda p: indexes[p] if p else -1
        lists = {}

        def displaylist(p):
            if not p:
                return -1
            if p not in lists:
                lists[p] = len(commands)
                for at in range(p, len(data), 8):
                    w0, w1 = struct.unpack_from('>II', data, at)
                    commands.append((w0, w1))
                    if w0 >> 24 == 0xb8:
                        break
                else:
                    raise AssertionError(path)
            return lists[p]

        rows = []
        for node in nodes:
            op, ro = half(node), ptr(node + 4)
            typ = op & 255
            matrix = extra = affects = other = primary = secondary = -1
            lo = hi = 0.
            if typ == 1:
                matrix = half(ro + 2)
            elif typ in (2, 3):
                matrix, extra = half(ro + 14), half(ro + 16)
            elif typ == 21:
                matrix = half(ro + 12)
            elif typ == 8:
                lo, hi = struct.unpack_from('>ff', data, ro)
                affects = index(ptr(ro + 8))
            elif typ == 9:
                affects, other = index(ptr(ro + 24)), index(ptr(ro + 28))
            elif typ == 18:
                affects = index(ptr(ro))
            elif typ in (4, 24):
                primary, secondary = displaylist(ptr(ro)), displaylist(ptr(ro + 4))
            rows.append('{' + ','.join(map(str, [op, index(ptr(node+8)), index(ptr(node+12)),
                index(ptr(node+20)), matrix, extra, affects, other])) + ',' + lo.hex() + 'f,'
                + hi.hex() + 'f,' + str(primary) + ',' + str(secondary) + '}')
        name = 'asset' + str(len(assets))
        output.append('static const NodeSpec ' + name + '[]={\n' + ',\n'.join(rows) + '\n};\n')
        assets.append('{"' + path.stem + '",' + name + ',' + str(len(nodes)) + ','
                      + str(index(root)) + ',' + args[5] + '}')
    output.append('static const Asset assets[]={\n' + ',\n'.join(assets) + '\n};\n')
    output.append('static const Gfx commands[]={\n' + ',\n'.join(
        '{{0x%08x,0x%08x}}' % c for c in commands) + '\n};\n')
    return ''.join(output)


def main():
    constants = (ROOT / 'src/bondconstants.h').read_text()
    model = (ROOT / 'src/game/model.c').read_text()
    source = re.search(r'typedef enum MODELNODE_OPCODE\s*\{.*?\}\s*\w+;', constants, re.S)[0] + '\n'
    source += (HERE / 'harness.h').read_text()
    matrix = (ROOT / 'src/game/matrixmath.c').read_text()
    source += function(matrix, 'matrix_4x4_set_identity')
    for name in ('modelFindNodeMtxIndex', 'modelFindNodeMtx'):
        source += function(model, name)
    lod = (ROOT / 'src/game/modellod.c').read_text()
    lod = re.sub(r'^#include[^\n]*\n', '', lod, flags=re.M)
    # Preserve segmented addresses while accommodating host pointers for the RAM arena.
    lod = lod.replace('u32 address;', 'uintptr_t address;').replace('address = (u32)source;', 'address = (uintptr_t)source;')
    lod = lod.replace('(u32)model', '(uintptr_t)model')
    source += lod
    quaternion = (ROOT / 'src/game/quaternion.c').read_text()
    source += function(quaternion, 'quaternion_set_rotation_around_xyzf')
    for name in ('matrix_4x4_set_rotation_around_y', 'matrix_column_3_scalar_multiply_2',
                 'matrix_4x4_set_position', 'matrix_4x4_multiply_homogeneous'):
        source += function(matrix, name)
    for name in ('modelBuildJointTransform', 'modelBuildQuaternionTransform',
                 'modelGetHalfRotationQuaternion', 'modelBuildSecondaryMatrix',
                 'modelBuildGroupMatrices', 'sub_GAME_7F06DB5C'):
        f = function(model, name)
        if name == 'modelBuildQuaternionTransform':
            f = f.replace('s32 i;', 's32 i; quaternionBuilds++;')
        if name == 'sub_GAME_7F06DB5C':
            f = f.replace('s32 *new_var;', 'intptr_t *new_var;').replace('s32 sp1C;', 'intptr_t sp1C;')
            f = f.replace('(s32)arg2->Parent', '(intptr_t)arg2->Parent').replace('(s32)&sp48[sp54]', '(intptr_t)&sp48[sp54]')
            f = f.replace('((void (*)(s32, s32, s32)) g_ModelJointPositionedFunc)(sp54, sp1C, sp1C);',
                          'g_ModelJointPositionedFunc(sp54, (Mtxf *)sp1C);')
        source += f
    source += fixtures() + (HERE / 'check.c').read_text()
    # Integration: production scope and invalidations must remain connected.
    tick = function((ROOT / 'src/game/chr.c').read_text(), 'chrTick')
    assert tick.index('modelLodBegin(model)') < tick.index('subcalcmatrices(&renderdata, model)') < tick.index('modelLodEnd()')
    for name in ('modelPromoteNodeOffsetsToPointers', 'modelNodeReplaceGdl'):
        assert 'modelLodResetCache()' in function(model, name)
    assert 'modelLodInvalidateInstance(model)' in function(model, 'modelInitRwData')
    props = (ROOT / 'src/game/propobj.c').read_text()
    for name in ('monitorProcessAndRender', 'objHideMonitorScreens'):
        assert 'modelLodInvalidateInstance(model)' in function(props, name)
    material = (ROOT / 'src/game/modelonecycle.c').read_text()
    for name in ('modelOneCycleResetCache', 'modelOneCycleInvalidateGdlRange'):
        assert 'modelLodResetCache()' in function(material, name)
    for section in ('text', 'data', 'rodata', 'bss'):
        assert f'modellod.o (.{section})' in (ROOT / f'ld/game.{section}.ld.inc').read_text()
    with tempfile.TemporaryDirectory(prefix='gud-character-lod-skips-') as tmp:
        work = Path(tmp)
        (work / 'check.c').write_text(source)
        cmd = shlex.split(os.environ.get('CC', 'cc')) + ['-std=c99', '-O2', '-g', '-Wall', '-Wextra', '-Werror',
            '-Wno-unused-variable', '-Wno-unused-function', '-Wno-incompatible-pointer-types',
            '-fno-strict-aliasing', '-ffp-contract=off']
        cmd += shlex.split(os.environ.get('TEST_CFLAGS', ''))
        subprocess.run(cmd + [str(work / 'check.c'), '-lm', '-o', str(work / 'check')], check=True)
        env = dict(os.environ, ASAN_OPTIONS='detect_leaks=0', UBSAN_OPTIONS='halt_on_error=1')
        subprocess.run([str(work / 'check')], env=env, check=True)


if __name__ == '__main__':
    main()
