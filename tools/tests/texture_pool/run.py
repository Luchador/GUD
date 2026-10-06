#!/usr/bin/env python3
"""Run the production texture allocator with bounded host RAM and DMA.

Optional argument: a concatenated GUTX image bank, to validate native records.
No ROM or extracted Nintendo assets are included in the test fixtures.
"""
from pathlib import Path
import os
import re
import shlex
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


def declaration(source, pattern):
    match = re.search(pattern, source, re.S)
    assert match, pattern
    return match[0] + '\n'


image = (ROOT / 'src/game/image.c').read_text()
image_h = (ROOT / 'src/game/image.h').read_text()
memp = (ROOT / 'src/memp.c').read_text()
source = (HERE.parent / 'render_options/harness.h').read_text().split('#define PLAYER_1')[0]
source += '#define ARRAYCOUNT(a) (sizeof(a)/sizeof((a)[0]))\n'
source += '#define bcopy(s,d,n) memmove(d,s,n)\n#define MAX_TEXTURES 4096\n'
source += '\n'.join(re.findall(r'^#define (?:TEXFORMAT_|RAW_TEXTURE_|TEX_ALPHA_WEIGHT|TEX_DATA_PREFIX_MAGIC).*$', image_h + '\n' + image, re.M)) + '\n'
for name in ('tex', 'texpool', 'texcacheitem'):
    source += declaration(image_h, r'struct ' + name + r'\s*\{.*?\n};')
for name in ('texdataprefix', 'texoverflow'):
    source += declaration(image, r'struct ' + name + r'\s*\{.*?\n};')
for name in ('g_TexFormatGbiMappings', 'g_TexFormatDepths', 'g_TexFormatLutModes'):
    source += declaration(image, r's32 ' + name + r'\[\]\s*=\s*\{.*?\n};')
source += '''
static union { u64 align; u8 data[8*1024*1024]; } g_Ram;
static struct texpool g_MainPool;
static struct texoverflow *g_TexOverflow;
static struct texcacheitem g_TexCacheItems[150];
static s32 g_TexCacheCount, g_TexNumToLoad;
/* The production code reads the packed big-endian word for this field. */
static u32 g_Textures[MAX_TEXTURES];
static struct {u32 imagesRomStart, textureCount;} g_TextureRomConfig = {0, MAX_TEXTURES};
static u8 *g_Rom;
static size_t g_RomSize;
static int g_PayloadCopies, g_StageAllocations;
typedef struct MemoryPool {u8 *start, *pos, *end, *prevpos;} MemoryPool;
static MemoryPool g_mempPools[7];
#define MEMPOOL_STAGE 4
#define osMemSize (sizeof(g_Ram.data))
#undef K0_TO_PHYS
#undef PHYS_TO_K0
#undef IS_KSEG0
#define K0_TO_PHYS(p) ((u32)(uintptr_t)(p))
#define PHYS_TO_K0(p) (g_Ram.data + (p))
#define IS_KSEG0(p) ((u8 *)(p) >= g_Ram.data && (u8 *)(p) < g_Ram.data + sizeof(g_Ram.data))
static u32 osVirtualToPhysical(void *p) {
    assert(IS_KSEG0(p)); return (u8 *)p - g_Ram.data;
}
static void romCopy(void *dst, void *address, u32 size) {
    size_t offset = (uintptr_t)address;
    assert(offset <= g_RomSize && size <= g_RomSize-offset);
    if (IS_KSEG0(dst)) g_PayloadCopies++;
    memcpy(dst, g_Rom+offset, size);
}
static void *mempAllocBytesInBank(u32 bytes, u8 bank) {
    MemoryPool *p = &g_mempPools[bank];
    assert(p->pos && p->pos + bytes <= p->end);
    u8 *result = p->pos; p->prevpos=result; p->pos+=bytes;
    g_StageAllocations++; return result;
}
'''
source += function(memp, 'mempTryAllocBytesInBank')
for name in ('texFindClosestColourIndexRGBA', 'texFindClosestColourIndexIA',
             'texShrinkPaletted', 'texShrinkNonPaletted', 'texSwapAltRowBytes',
             'texReadRawU16', 'texReadRawU32', 'texRawLevelBytes', 'texRawAllocationBytes',
             'texShouldWriteLodCache', 'texCommitLodCache', 'texHasBinaryAlpha',
             'texLoadRaw', 'texInitPool', 'texFindInPool', 'texFindByData',
             'texFreeBytesInBuffer', 'texLoad'):
    body = function(image, name)
    # Host pointer width/address emulation; allocator and loader logic are intact.
    body = body.replace('&ptr_texture_alloc_start', '&g_MainPool')
    for value in ('headerBuffer', 'memory', 'arg0->rightpos', 'arg0->leftpos', 'tex'):
        body = body.replace('(u32)' + value, '(uintptr_t)' + value)
    body = body.replace('K0_TO_PHYS(tex)', 'osVirtualToPhysical(tex)')
    source += body
source += (HERE / 'check.c').read_text()

with tempfile.TemporaryDirectory(prefix='gud-texture-pool-') as directory:
    work = Path(directory)
    (work / 'check.c').write_text(source)
    command = shlex.split(os.environ.get('CC', 'cc'))
    command += ['-std=c99', '-O1', '-g', '-Wall', '-Wextra', '-Wno-sign-compare',
                '-Wno-pointer-to-int-cast', '-Wno-int-to-pointer-cast',
                '-Wno-unused-variable', '-fsanitize=address,undefined',
                '-idirafter', str(ROOT / 'include')]
    subprocess.run(command + [str(work/'check.c'), '-o', str(work/'check')], check=True)
    subprocess.run([str(work/'check')] + sys.argv[1:], check=True,
                   env=dict(os.environ, ASAN_OPTIONS='detect_leaks=0', UBSAN_OPTIONS='halt_on_error=1'))
