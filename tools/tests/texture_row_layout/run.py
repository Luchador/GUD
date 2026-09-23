#!/usr/bin/env python3
"""Exercise GEditor encoding, the runtime loader and real RDP upload commands.

Host adaptations only replace DMA/address lookup and big-endian struct-word
access. The small TMEM model checks byte ordering, not full rasterization.
"""
from pathlib import Path
import os
import re
import shlex
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


def declaration(source, pattern):
    match = re.search(pattern, source, re.S)
    assert match, pattern
    return match[0] + '\n'


image = (ROOT / 'src/game/image.c').read_text()
image_h = (ROOT / 'src/game/image.h').read_text()
tex = Path(os.environ.get('TEX_SOURCE', ROOT / 'src/game/tex.c')).read_text()
tex_h = (ROOT / 'src/game/tex.h').read_text()
source = (HERE.parent / 'render_options/harness.h').read_text().split('#define PLAYER_1')[0]
source += '#include <limits.h>\n#define ARRAYCOUNT(a) (sizeof(a) / sizeof((a)[0]))\n'
source += '#define bcopy(s,d,n) memmove(d,s,n)\n#define TEX_ALPHA_WEIGHT 961\n#define G_ON 1\n'
source += '\n'.join(re.findall(r'^#define TEXFORMAT_.*$', image_h, re.M)) + '\n'
source += '\n'.join(re.findall(r'^#define RAW_TEXTURE_.*$', image, re.M)) + '\n'
for name in ('tex', 'texpool', 'texcacheitem', 'image_entry'):
    source += declaration(image_h, r'struct ' + name + r'\s*\{.*?\n};')
for name in ('s_bss_8008D148', 's_bss_8008D2A8'):
    source += declaration(tex_h, r'typedef struct ' + name + r'\s*\{.*?\n} ' + name + ';')
source += declaration((ROOT / 'src/bondtypes.h').read_text(),
                      r'typedef struct sImageTableEntry\s*\{.*?\n} sImageTableEntry;')
source += declaration((ROOT / 'src/bondconstants.h').read_text(),
                      r'typedef enum TEXTURE_RENDER_STYLE\s*\{.*?\n} TEXTURE_RENDER_STYLE;')
source += '#include <gbi_extension.h>\n'
source += '''
static struct texcacheitem g_TexCacheItems[150];
static s32 g_TexCacheCount, g_TexLutMode;
static s_bss_8008D148 g_TexTileStates[8];
static s_bss_8008D2A8 g_TexTileSizes[8];
static struct image_entry g_Textures[4096];
static const u8 *g_Record;
static size_t g_RecordSize;
static struct tex *g_Lookup;
#define NUM_TEXTURES 4096u
static void romCopy(void *dst, const void *address, s32 size) {
    size_t offset = (uintptr_t)address;
    assert(size >= 0 && offset + size <= g_RecordSize);
    memcpy(dst, g_Record + offset, size);
}
static struct tex *texFindByData(u32 address) {
    assert(address == 0x10000); return g_Lookup;
}
static void texLoad(s32 *address, struct texpool *pool) {
    (void)address; (void)pool; abort(); /* Fixtures already reside in the pool. */
}
typedef u32 DWORD;
typedef int BOOL;
typedef uint64_t ULONGLONG;
typedef struct TexPixel { u8 r, g, b, a; } TexPixel;
typedef struct TexImportOptions { u8 format, mipmaps, hitsound, hittexture; } TexImportOptions;
'''
source += re.sub(r'^#include[^\n]*\n', '',
                 (ROOT / 'tools/geditor/src/texencode.c').read_text(), flags=re.M)
for name in ('g_TexFormatGbiMappings', 'g_TexFormatDepths', 'g_TexFormatLutModes'):
    source += declaration(image, r's32 ' + name + r'\[\]\s*=\s*\{.*?\n};')
for name in ('texFindClosestColourIndexRGBA', 'texFindClosestColourIndexIA',
             'texShrinkPaletted', 'texShrinkNonPaletted', 'texSwapAltRowBytes',
             'texReadRawU16', 'texReadRawU32', 'texShouldWriteLodCache',
             'texCommitLodCache', 'texHasBinaryAlpha', 'texLoadRaw'):
    source += function(image, name)
for name in ('sub_GAME_7F0CC4C8', 'texSetLutMode', 'texTrySetTileState', 'texTrySetTileSize',
             'texGetWidthAtLod', 'texGetHeightAtLod', 'texGetLineSizeInBytes',
             'texGetSizeInBytes', 'texGetDepthAndSize', 'texDimensionToMask',
             'texModeToGbiMode', 'sub_GAME_7F0CCAFC', 'sub_GAME_7F0CCB38',
             'texWriteTileFromDefinition', 'texWriteLoadToTmemAddr',
             'texWriteTileLods', 'texWriteLoadToTmemZero', 'texWriteTile',
             'texHandleType2', 'texHandleType1', 'texHandleType0',
             'texHandleType4', 'texHandleType3', 'is_less_than_certain_power_of_2',
             'ceil8000', 'ceil4000', 'ceil2000', 'ceil1000',
             'sub_GAME_7F0767D8', 'sub_GAME_7F076848', 'sub_GAME_7F0768B8',
             'sub_GAME_7F076928', 'texSetRenderMode', 'texSelect'):
    body = function(tex, name)
    # Original code reads the low bits of a packed big-endian descriptor word.
    # On the host, read the same depth field by name, without changing its value.
    body = body.replace('arg0[2] & 3', '((struct tex *)arg0)->depth')
    source += body
source += (HERE / 'check.c').read_text()

with tempfile.TemporaryDirectory(prefix='gud-texture-layout-') as directory:
    work = Path(directory)
    (work / 'check.c').write_text(source)
    (work / 'PR').mkdir()
    gbi = (ROOT / 'include/PR/gbi.h').read_text()
    gbi = gbi.replace('uintptr_t w0;', 'u32 w0;').replace('uintptr_t w1;', 'u32 w1;')
    for arg in ('m1a', 'm1b', 'm2a', 'm2b'):
        gbi = gbi.replace(f'({arg}) <<', f'((u32)({arg})) <<')
    (work / 'PR/gbi.h').write_text(gbi)
    command = shlex.split(os.environ.get('CC', 'cc'))
    command += ['-std=c99', '-O1', '-g', '-Wall', '-Wextra', '-Wno-sign-compare',
                '-Wno-pointer-to-int-cast', '-Wno-int-to-pointer-cast',
                '-Wno-incompatible-pointer-types', '-Wno-unused-variable',
                '-fsanitize=address,undefined', '-I', str(work),
                '-idirafter', str(ROOT / 'include')]
    subprocess.run(command + [str(work / 'check.c'), '-o', str(work / 'check')], check=True)
    subprocess.run([str(work / 'check')], check=True,
                   env=dict(os.environ, ASAN_OPTIONS='detect_leaks=0', UBSAN_OPTIONS='halt_on_error=1'))
