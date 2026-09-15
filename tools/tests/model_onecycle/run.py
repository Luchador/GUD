#!/usr/bin/env python3
"""Exercise actual model setup, conversion, draw dispatch and cache lifetime."""
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


def strip_includes(text):
    return re.sub(r'^#include[^\n]*\n', '', text, flags=re.M)


def function(source, name):
    match = re.search(r'^(?:static )?[\w *]+\b' + name + r'\([^;]*?\)\s*\{', source, re.M)
    assert match, name
    end = source.index('{', match.start()) + 1
    depth = 1
    while depth:
        depth += (source[end] == '{') - (source[end] == '}')
        end += 1
    return source[match.start():end] + '\n'


source = (HERE.parent / 'render_options/harness.h').read_text().split('static u32 g_TestButtons;')[0]
source += strip_includes((ROOT / 'src/game/renderconfig.h').read_text())
config = strip_includes((ROOT / 'src/game/renderconfig.c').read_text())
config = config.replace('(Gfx *)(physical | 0x80000000)', '(Gfx *)(g_TestRam + physical)')
config = config.replace('((u32)cmd & 0x1fffffff)', '((u8 *)cmd - g_TestRam)')
source += config
source += (ROOT / 'src/bgtransparency.h').read_text()
source += strip_includes((ROOT / 'src/game/bgonecycle.h').read_text())
image_header = (ROOT / 'src/game/image.h').read_text()
source += re.search(r'struct tex \{.*?\n};', image_header, re.S)[0] + '\n'
source += '''
static struct tex g_TestTexture;
static struct tex *texFindByData(u32 address) {
    return address == 0x123450 ? &g_TestTexture : NULL;
}
'''
source += strip_includes((ROOT / 'src/game/bgonecycle.c').read_text())
source += (HERE / 'harness.h').read_text()
source += re.search(r'^#define MODEL_RENDER_FIRST_PERSON[^\n]*', (ROOT / 'src/game/model.h').read_text(), re.M)[0] + '\n'
model = (ROOT / 'src/game/model.c').read_text()
for name in ('modelApplyRenderModeType1', 'modelApplyRenderModeType2',
             'modelApplyRenderModeType3', 'modelApplyRenderModeType4', 'modelApplyCullMode'):
    source += function(model, name)
cache = strip_includes((ROOT / 'src/game/modelonecycle.c').read_text())
# Pointer arithmetic and the hash use host pointer width; N64 GBI stays 8 bytes.
cache = re.sub(r'\(u32\)(start|end|primary|source|entry->source)', r'(uintptr_t)\1', cache)
source += cache
source += function(model, 'modelRenderNodeGundl')
source += function(model, 'modelRenderNodeDlWithCache')
# Independently decode the final RDP state using the established BG test oracle.
bgcheck = (HERE.parent / 'bg_onecycle/check.c').read_text()
source += bgcheck[:bgcheck.index('static const Gfx standard[]')]
source += (HERE / 'check.c').read_text()

gunfire = (ROOT / 'src/game/gunfire.c').read_text()
assert 'renderdata.flags |= MODEL_RENDER_FIRST_PERSON;' in function(gunfire, 'gunRenderFirstPersonGunModels')
for name in ('watchRenderItemModel', 'watchRenderController', 'sub_GAME_7F068EC4'):
    assert 'MODEL_RENDER_FIRST_PERSON' not in function(gunfire, name)

assert 'modelOneCycleResetCache()' in function((ROOT / 'src/game/dyn.c').read_text(), 'dynInitMemory')
assert 'modelOneCycleInvalidateGdlRange(dst, out)' in function((ROOT / 'src/game/tex.c').read_text(), 'texLoadFromGdl')
boss = (ROOT / 'src/boss.c').read_text()
assert boss.index('memaReset(') < boss.index('dynInitMemory()')
assert 'while (g_MainStageNum < 0 || pendingGfx != 0)' in boss
for section in ('text', 'data', 'rodata', 'bss'):
    assert f'modelonecycle.o (.{section})' in (ROOT / f'ld/game.{section}.ld.inc').read_text()


def asset_streams(kind):
    """Walk real model nodes, retaining every authored state/geometry command.

    Texture markers become representative expanded uploads plus gSPTexture.
    Full texture allocation/TMEM contents and rasterization are not emulated.
    """
    for path in sorted((ROOT / f'assets/obseg/{kind}').glob('*Z.bin')):
        if kind == 'chr' and path.stem != 'Csuit_lf_handZ':
            continue
        name = path.stem[1:-1]
        header = ROOT / f'assets/obseg/{kind}/{name}/ModelFileHeader.inc.c'
        if kind == 'chr':
            header = header.with_name('modelFileHeader.inc.c')
        if not header.exists():
            continue
        args = re.search(r'MODELFILEHEADER\((.*)\)', header.read_text())[1].split(',')
        root = int(args[4], 0) * 4 + int(args[8], 0) * 12
        data = path.read_bytes()
        if data[:2] == b'\x11\x72':
            data = zlib.decompress(data[2:], -15)

        def word(off):
            return struct.unpack_from('>I', data, off)[0]

        todo, visited = [root], set()
        while todo:
            node = todo.pop()
            if node in visited or node + 24 > len(data):
                continue
            visited.add(node)
            opcode = struct.unpack_from('>H', data, node)[0] & 255
            for delta in (12, 20):
                ptr = word(node + delta)
                if ptr:
                    todo.append(ptr & 0xffffff)
            ro = word(node + 4) & 0xffffff
            for delta in ({8: (8,), 9: (24, 28), 18: (0,)}.get(opcode, ())):
                if word(ro + delta):
                    todo.append(word(ro + delta) & 0xffffff)
            if opcode not in (4, 0x18):
                continue
            ro = word(node + 4) & 0xffffff
            modeltype = data[ro + 0x12] if opcode == 4 else struct.unpack_from('>H', data, ro + 0x18)[0]
            if modeltype not in (2, 3, 4) or not word(ro):
                continue
            commands = []
            texture_command = (0xbb000001, 0xffffffff)
            for off in range(word(ro) & 0xffffff, len(data) - 7, 8):
                w0, w1 = struct.unpack_from('>II', data, off)
                if w0 >> 24 == 0xbb:
                    texture_command = (w0, w1)
                if w0 >> 24 == 0xc0:
                    if (w1 & 0xfff) in (1508, 1511):
                        commands.append((0x06000000, 0x100))  # water helper call
                    else:
                        commands += [texture_command, (0xfd100000, 0x70000),
                                     (0xf5100000, 0x07000000), (0xe6000000, 0),
                                     (0xf3000000, 0x073ff000), (0xe7000000, 0)]
                else:
                    commands.append((w0, w1))
                if w0 >> 24 == 0xb8:
                    break
            yield name if kind == 'prop' else kind + '/' + name, modeltype, commands


with tempfile.TemporaryDirectory(prefix='gud-model-onecycle-') as directory:
    work = Path(directory)
    (work / 'check.c').write_text(source)
    (work / 'PR').mkdir()
    gbi = (ROOT / 'include/PR/gbi.h').read_text()
    gbi = gbi.replace('uintptr_t w0;', 'u32 w0;').replace('uintptr_t w1;', 'u32 w1;')
    for arg in ('m1a', 'm1b', 'm2a', 'm2b'):
        gbi = gbi.replace(f'({arg}) <<', f'((u32)({arg})) <<')
    (work / 'PR/gbi.h').write_text(gbi)
    command = shlex.split(os.environ.get('CC', 'cc'))
    command += ['-std=c99', '-O2', '-Wall', '-Wextra', '-Werror', '-Wno-sign-compare',
                '-Wno-pointer-to-int-cast', '-Wno-int-to-pointer-cast', '-Wno-missing-braces',
                '-I', str(work), '-idirafter', str(ROOT / 'include')]
    command += shlex.split(os.environ.get('TEST_CFLAGS', ''))
    command += [str(work / 'check.c'), '-o', str(work / 'check')]
    subprocess.run(command, check=True)
    subprocess.run([str(work / 'check')], check=True)
    fixture = bytearray()
    for name, modeltype, commands in (entry for kind in ('prop', 'gun', 'chr') for entry in asset_streams(kind)):
        name = name.encode()
        fixture += struct.pack('>III', len(name), modeltype, len(commands)) + name
        for w0, w1 in commands:
            fixture += struct.pack('>II', w0, w1)
    (work / 'props.bin').write_bytes(fixture)
    subprocess.run([str(work / 'check'), str(work / 'props.bin')], check=True)
