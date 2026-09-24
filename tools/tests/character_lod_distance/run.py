#!/usr/bin/env python3
"""Exercise stage selection and the real character-scoped LOD calculation."""
from pathlib import Path
import os
import re
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[3]
def function(source, name):
    match = re.search(r'^(?:static )?[\w *]+\b' + name + r'\([^;]*?\)\s*\{', source, re.M)
    assert match, name
    end = source.index('{', match.start()) + 1
    depth = 1
    while depth:
        depth += (source[end] == '{') - (source[end] == '}')
        end += 1
    return source[match.start():end] + '\n'

lv = (ROOT / 'src/game/lv.c').read_text()
model = (ROOT / 'src/game/model.c').read_text()
chr_tick = function((ROOT / 'src/game/chr.c').read_text(), 'chrTick')
source = r'''
#include <assert.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stddef.h>
typedef float f32;typedef int32_t s32;typedef uint32_t u32;
#define TRUE 1
#define FALSE 0
#define ARRAYCOUNT(a) ((s32)(sizeof(a)/sizeof(*(a))))
#define ENVIRONMENTDATA_PLAYERS_4 400
enum LEVELID { LEVELID_TITLE=90 };
struct LevelEntry { s32 levelID; f32 chrLODDistance; };
static struct LevelEntry g_LevelInfoTable[]={{29,.8f},{429,2},{90,.5f},{22,1.25f}};
static int players=1,lookups;
static s32 getPlayerCount(void) {lookups++;return players;}
static s32 g_CurrentStageToLoad;
struct player_data { int unused; };
typedef struct Mtxf {f32 m[4][4];} Mtxf;
typedef struct ModelNode ModelNode;
union ModelRoData { struct { f32 MinDistance,MaxDistance; ModelNode *Affects; } LOD; };
union ModelRwData { struct { int visible; } LOD; };
struct ModelNode { union ModelRoData *Data; ModelNode *Child; union ModelRwData rw; };
typedef struct Model { f32 scale; Mtxf matrix; } Model;
static union ModelRwData *modelGetNodeRwData(Model *model,ModelNode *node) {(void)model;return &node->rw;}
static Mtxf *modelFindNodeMtx(Model *model,ModelNode *node,int index) {(void)node;(void)index;return &model->matrix;}
static f32 viewscale=1,g_ModelDistanceScale=1;
static s32 g_ModelDistanceDisabled;
static f32 getPlayer_c_lodscalez(void) {return viewscale;}
'''
source += re.search(r'^f32 g_ChrLodDistance[^;]*;', lv, re.M)[0] + '\n'
source += function(lv, 'lvFindLevelInfo') + function(lv, 'lvFindStageInfo')
# Run the actual stage-loader prefix, stopping before unrelated level loading.
stage = function(lv, 'lvlStageLoad')
source += stage[:stage.index('    g_BgRenderEnabled =')] + '}\n'
source += function(model, 'modelSetDistanceScale') + function(model, 'modelUpdateDistanceRelations')
source += r'''
static ModelNode mesh,high,low;
static union ModelRoData highData={.LOD={0,1000,&mesh}},lowData={.LOD={1000,100000,&mesh}};
static void subcalcmatrices(void *unused,Model *model) {
    (void)unused;modelUpdateDistanceRelations(model,&high);modelUpdateDistanceRelations(model,&low);
}
static void modelLodBegin(Model *model) {(void)model;}
static void modelLodEnd(void) {}
static void character(Model *model) { int renderdata=0;
'''
start = chr_tick.index('        modelSetDistanceScale(g_ChrLodDistance);')
end = chr_tick.index('        modelSetDistanceScale(1.0f);', start) + len('        modelSetDistanceScale(1.0f);')
source += chr_tick[start:end] + '\n}\n'
source += r'''
int main(void) {
    Model model={.scale=1};high.Data=&highData;low.Data=&lowData;
    const float factors[]={.5f,.8f,1,1.375f,2};
    for(unsigned f=0;f<sizeof(factors)/sizeof(*factors);f++) {
        g_LevelInfoTable[0].chrLODDistance=factors[f];players=1;lvlStageLoad(29);
        assert(g_ChrLodDistance==factors[f]);int before=lookups;
        for(int zoom=0;zoom<2;zoom++) {
            viewscale=zoom?.5f:1;
            float boundary=1000/(factors[f]*viewscale);
            for(int side=-1;side<=1;side+=2) {
                model.matrix.m[3][2]=-(boundary+side);character(&model);
                assert(high.rw.LOD.visible==(side<0)&&low.rw.LOD.visible==(side>0));
                assert(g_ModelDistanceScale==1&&lookups==before);
            }
        }
        // A following ordinary prop still uses the unmodified model distance.
        viewscale=1;model.matrix.m[3][2]=-900;modelUpdateDistanceRelations(&model,&high);
        assert(high.rw.LOD.visible);
    }
    players=4;lvlStageLoad(29);assert(g_ChrLodDistance==2);
    lvlStageLoad(22);assert(g_ChrLodDistance==1.25f); // MP falls back to solo row
    lvlStageLoad(LEVELID_TITLE);assert(g_ChrLodDistance==.5f);
    lvlStageLoad(999);assert(g_ChrLodDistance==1);
    const float invalid[]={0,-1,NAN,INFINITY,-INFINITY};players=1;
    for(unsigned i=0;i<sizeof(invalid)/sizeof(*invalid);i++) {
        g_LevelInfoTable[0].chrLODDistance=invalid[i];lvlStageLoad(29);assert(g_ChrLodDistance==1);
    }
    puts("PASS: stage/MP/Title LOD selection, scoped character thresholds at five factors and two view scales, no per-character lookups, prop isolation, invalid-value fallback.");
}
'''
with tempfile.TemporaryDirectory(prefix='gud-character-lod-distance-') as temp:
    work = Path(temp)
    (work / 'check.c').write_text(source)
    subprocess.run([os.environ.get('CC', 'cc'), '-std=c99', '-O1', '-g', '-Wall', '-Wextra',
        '-Werror', '-Wno-sign-compare', '-Wno-unused-variable', '-fsanitize=address,undefined', str(work / 'check.c'),
        '-lm', '-o', str(work / 'check')], check=True)
    subprocess.run([str(work / 'check')], check=True,
        env=dict(os.environ, ASAN_OPTIONS='detect_leaks=0', UBSAN_OPTIONS='halt_on_error=1'))
