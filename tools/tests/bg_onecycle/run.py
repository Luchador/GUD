#!/usr/bin/env python3
"""Check the actual converter, AA walker and room-cache lifecycle on the host."""
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
bg = (ROOT / 'src/game/bg.c').read_text()
for name in ('DL_LUT_PRIMARY', 'DL_LUT_PRIMARY_ADDFOG', 'DL_LUT_SECONDARY', 'DL_LUT_SECONDARY_ADDFOG'):
    source += re.search(r'Gfx ' + name + r'\[\].*?\n};', bg, re.S)[0] + '\n'
source += (HERE / 'harness.h').read_text()
for name in ('bgBuildRoomOneCycleGdl', 'bgFreeRoomData', 'bgRenderRoomPrimary', 'bgRenderRoomSecondary'):
    source += function(bg, name)
source += '\n'.join(re.findall(r'^#define TEXFORMAT_.*$', image_header, re.M)) + '\n'
source += function((ROOT / 'src/game/image.c').read_text(), 'texHasBinaryAlpha')
extension = (ROOT / 'include/gbi_extension.h').read_text()
source += re.search(r'typedef enum\s*\{[^}]*\}\s*TextureTypes;', extension, re.S)[0] + '\n'
source += (HERE / 'texture_markers.h').read_text()
# The N64 uses big-endian command bytes and 32-bit pointers. Adapt only these
# host representations; keep production marker/texture/light dispatch intact.
expander = function((ROOT / 'src/game/tex.c').read_text(), 'texLoadFromGdl')
expander = expander.replace('switch (*(u8 *)in)', 'switch (in->words.w0 >> 24)')
expander = expander.replace('((s32)out) - ((s32)dst)', '(s32)((u8 *)out - (u8 *)dst)')
expander = re.sub(r'    s32\s+pad;\n', '', expander)
source += expander
source += (HERE / 'check.c').read_text()
source += (HERE / 'cutouts.c').read_text()

# Classification precedes row swaps/mip generation, using the existing spare
# descriptor bit (no N64 layout growth or change to texture pointer prefixes).
loader = function((ROOT / 'src/game/image.c').read_text(), 'texLoadRaw')
assert loader.index('tex->hasBinaryAlpha = FALSE') < loader.index('romCopy(')
assert loader.index('romCopy(') < loader.index('texHasBinaryAlpha(') < loader.index('texSwapAltRowBytes(')
assert 'u32 hasBinaryAlpha : 1' in image_header

# Check the real integration ordering, including all manually ordered sections.
load = function(bg, 'bgLoadRoomModelData')
assert load.rindex('bgApplyDynamicCCRMLUT(') < load.index('bgBuildRoomVtxBounds(roomID)') < load.index('bgBuildRoomOneCycleGdl(roomID)')
assert 'g_BgOneCycleRooms[i].gdl = NULL' in function(bg, 'bgLoadFile')
assert 'g_BgOneCycleRooms[i].secondaryGdl = NULL' in function(bg, 'bgLoadFile')
assert 'BG_CUTOUT_THRESHOLD' in function(bg, 'bgRenderRoomSecondary')
for section in ('text', 'data', 'rodata', 'bss'):
    assert f'bgonecycle.o (.{section})' in (ROOT / f'ld/game.{section}.ld.inc').read_text()

with tempfile.TemporaryDirectory(prefix='gud-bg-onecycle-') as directory:
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

    # Real authored primary streams, passed through the actual runtime LUTs.
    # Texture allocation/upload is not emulated: ordinary markers become NOOPs,
    # animated-water markers become calls (as texLoadFromGdl emits in game).
    # This checks material/state coverage, not texel sampling or rasterization.
    for name in ('bg_run_all_p.c', 'bg_depo_all_p.c', 'bg_tra_all_p.c',
                 'bg_dest_all_p.c', 'u/bg_jun_all_p.c', 'bg_dam_all_p.c', 'bg_cave_all_p.c'):
        asset = (ROOT / 'assets/obseg/bg' / name).read_text()
        fixture = bytearray()
        for body in re.findall(r'u32 pri_mapping_binary_\d+\[\] = \{(.*?)\};', asset, re.S):
            data = bytes.fromhex(''.join(re.findall(r'0x([0-9a-fA-F]{8})', body)))
            if data[:2] != b'\x11\x72':
                continue
            data = bytearray(zlib.decompress(data[2:], -15))
            for offset in range(0, len(data), 8):
                w0, w1 = struct.unpack_from('>II', data, offset)
                if w0 >> 24 == 0xc0:
                    water = (w1 & 0xfff) in (1508, 1511)
                    struct.pack_into('>II', data, offset, 0x06000000 if water else 0, 0x100 if water else 0)
            fixture += struct.pack('>I', len(data)) + data
        path = work / (Path(name).stem + '.bin')
        path.write_bytes(fixture)
        subprocess.run([str(work / 'check'), str(path)], check=True)

        # Secondary room commands/LUTs are real. Use a representative full
        # RGBA16 upload for ordinary markers because ROM texture payloads are
        # unavailable here. This measures candidate coverage, not actual texels.
        fixture = bytearray()
        for body in re.findall(r'u32 sec_mapping_binary_\d+\[\] = \{(.*?)\};', asset, re.S):
            data = bytes.fromhex(''.join(re.findall(r'0x([0-9a-fA-F]{8})', body)))
            if data[:2] != b'\x11\x72':
                continue
            commands = []
            for w0, w1 in struct.iter_unpack('>II', zlib.decompress(data[2:], -15)):
                if w0 >> 24 == 0xc0:
                    if (w1 & 0xfff) in (1508, 1511) or (w0 & 7) == 1:
                        commands.append((0x06000000, 0x100))
                    else:
                        commands += [(0xfd100000, 0x123450), (0xf5100000, 0x07000000),
                                     (0xe6000000, 0), (0xf3000000, 0x073ff000),
                                     (0xe7000000, 0), (0xf5101000, 0), (0xba000e02, 0)]
                else:
                    commands.append((w0, w1))
            data = b''.join(struct.pack('>II', *command) for command in commands)
            fixture += struct.pack('>I', len(data)) + data
        path = work / (Path(name).stem + '-secondary.bin')
        path.write_bytes(fixture)
        subprocess.run([str(work / 'check'), str(path), 'secondary'], check=True)
