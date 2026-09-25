#!/usr/bin/env python3
"""Run the production shadow renderer and material expansion on a host fixture."""
import os
from pathlib import Path
import re
import subprocess
import sys
import tempfile

HERE = Path(__file__).resolve().parent
ROOT = HERE.parents[3]
TESTS = ROOT / 'tools/tests'
def strip(text):
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

source = (TESTS / 'render_options/harness.h').read_text().split('static u32 g_TestButtons;')[0]
source += '#include <math.h>\n'
source += (ROOT / 'src/bgtransparency.h').read_text()
source += (ROOT / 'src/doorshadowformat.h').read_text()
source += (ROOT / 'src/propconstants.h').read_text()
source += 'static void renderRestoreDisplayListSettings(Gfx *a,Gfx *b) {(void)a;(void)b;}\n'
source += 'static void renderInvalidateDisplayListCache(void) {}\n'
config = (ROOT / 'src/game/renderconfig.c').read_text()
source += config[config.index('#define AA_FIRST_BLENDER_MASK'):config.index('static Gfx *g_RenderLeafCache')]
source += function(config, 'renderDisableAaCommand')
source += function(config, 'renderGetAaOffCommand')
bg = (ROOT / 'src/game/bg.c').read_text()
assert function(bg, 'bgLoadFile').index('doorShadowReset()') < function(bg, 'bgLoadFile').index('bgRoomCalcBB(')
for name in ('DL_LUT_PRIMARY', 'DL_LUT_PRIMARY_ADDFOG', 'DL_LUT_SECONDARY', 'DL_LUT_SECONDARY_ADDFOG'):
    source += re.search(r'Gfx ' + name + r'\[\].*?\n};', bg, re.S)[0] + '\n'
source += re.search(r'enum CCRMLUT\s*\{.*?\n};', (ROOT / 'src/bondconstants.h').read_text(), re.S)[0] + '\n'
source += '''
static Gfx *ptrDynamic_CC_RM_LUT[] = {
    NULL, DL_LUT_PRIMARY_ADDFOG, NULL, NULL, NULL,
    DL_LUT_SECONDARY_ADDFOG, DL_LUT_PRIMARY, DL_LUT_SECONDARY
};
static struct TestEnvironment { int FogEnabled; } g_TestEnvironment;
static struct TestEnvironment *envGetCurrent(void) { return &g_TestEnvironment; }
'''
source += strip((ROOT / 'src/game/bgapply.c').read_text())
image = (ROOT / 'src/game/image.h').read_text()
source += re.search(r'struct tex \{.*?\n};', image, re.S)[0] + '\n'
source += 'static struct tex g_TestTexture;\n'
source += 'static struct tex *texFindByData(u32 address) { (void)address; return &g_TestTexture; }\n'
source += strip((ROOT / 'src/game/bgonecycle.h').read_text())
source += strip((ROOT / 'src/game/bgonecycle.c').read_text())
source += '\n'.join(re.findall(r'^#define TEXFORMAT_.*$', image, re.M)) + '\n'
source += re.search(r'typedef enum\s*\{[^}]*\}\s*TextureTypes;', (ROOT / 'include/gbi_extension.h').read_text(), re.S)[0] + '\n'
source += (TESTS / 'bg_onecycle/texture_markers.h').read_text()
expander = function((ROOT / 'src/game/tex.c').read_text(), 'texLoadFromGdl')
expander = expander.replace('switch (*(u8 *)in)', 'switch (in->words.w0 >> 24)')
expander = expander.replace('((s32)out) - ((s32)dst)', '(s32)((u8 *)out - (u8 *)dst)')
expander = re.sub(r'    s32\s+pad;\n', '', expander)
source += expander
source += (HERE / 'runtime_harness.h').read_text()
source += '''
void doorShadowClearRenderCaches(void);
static void bgClearRoomRenderCaches(void) { doorShadowClearRenderCaches(); }
static void modelOneCycleResetCache(void) {}
'''
assert 'doorShadowClearRenderCaches();' in function(bg, 'bgClearRoomRenderCaches')
source += strip((ROOT / 'src/game/rendercache.c').read_text())
source += strip((ROOT / 'src/game/line_tri_intersect.c').read_text())
matrixmath = (ROOT / 'src/game/matrixmath.c').read_text()
source += function(matrixmath, 'matrix_4x4_set_identity')
# Keep the production converter; express its bit packing with unsigned shifts
# to avoid the original signed-shift UB in the host sanitizer build.
source += function(matrixmath, 'matrix_4x4_f32_to_s32').replace('(e1 << 16)', '((u32)e1 << 16)')
source += strip((ROOT / 'src/game/doorshadowmath.c').read_text())
source += strip((ROOT / 'src/game/doorshadow.c').read_text()).replace('(s32)memory', '(intptr_t)memory')
source += (HERE / 'collision_harness.h').read_text()
source += function(bg, 'bgTestRayIntersectsBbox')
collision = function(bg, 'bgTestBulletHitBackground')
# Only adapt byte reads of native big-endian display-list words.
collision = collision.replace('((u8 *)((Gfx*)point))[0]', '(((Gfx*)point)->words.w0 >> 24)')
collision = collision.replace('((u8 *)((Gfx*)point))[1]', '((((Gfx*)point)->words.w0 >> 16) & 255)')
source += collision
chrprop = (ROOT / 'src/game/chrprop.c').read_text()
source += function(chrprop, 'chrpropRayIntersectsRoomBbox')
source += function(chrprop, 'chrpropFindNearestBgHit')
source += (HERE / 'collision_check.c').read_text()
source += (HERE / 'render_check.c').read_text()
source += (HERE / 'runtime_check.c').read_text()
with tempfile.TemporaryDirectory(prefix='gud-door-shadow-') as directory:
    work = Path(directory)
    (work / 'check.c').write_text(source)
    (work / 'PR').mkdir()
    gbi = (ROOT / 'include/PR/gbi.h').read_text().replace('uintptr_t w0;', 'u32 w0;').replace('uintptr_t w1;', 'u32 w1;')
    for arg in ('m1a', 'm1b', 'm2a', 'm2b'):
        gbi = gbi.replace(f'({arg}) <<', f'((u32)({arg})) <<')
    (work / 'PR/gbi.h').write_text(gbi)
    subprocess.run([os.environ.get('CC','cc'), '-std=c99', '-O1', '-g', '-Wall', '-Wextra', '-Werror',
        '-Wno-sign-compare', '-Wno-pointer-to-int-cast', '-Wno-int-to-pointer-cast', '-Wno-missing-braces',
        '-Wno-unused-function', '-Wno-unused-variable', '-fsanitize=address,undefined', '-I', str(work),
        '-idirafter', str(ROOT / 'include'), str(work / 'check.c'), '-lm', '-o', str(work / 'check')], check=True)
    subprocess.run([str(work / 'check')] + sys.argv[1:], check=True,
        env=dict(os.environ, ASAN_OPTIONS='detect_leaks=0', UBSAN_OPTIONS='halt_on_error=1'))
