#!/usr/bin/env python3
"""Check saved guard flags and the production character distance cutoff."""
import os
from pathlib import Path
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


constants = (ROOT / 'src/bondconstants.h').read_text()
character = (ROOT / 'src/game/chr.c').read_text()
source = '#include <assert.h>\n#include <stdint.h>\n#include <stdio.h>\n#include <math.h>\n'
source += re.search(r'typedef enum GUARD_SETUP_FLAG.*?\} GUARD_SETUP_FLAG;', constants, re.S)[0] + '\n'
for name in ('CHRFLAG_NOFADE', 'CHRFLAG_CLONE', 'CHRFLAG_INVINCIBLE'):
    value = re.search(r'\b' + name + r'\s*=\s*(0x[0-9a-fA-F]+|\d+)', constants)[1]
    source += f'#define {name} {value}\n'
source += r'''
typedef int32_t s32; typedef uint32_t u32; typedef int16_t s16;
typedef uint16_t u16; typedef int8_t s8; typedef float f32;
typedef struct coord3d { union { struct { float x,y,z; }; float f[3]; }; } coord3d;
typedef struct Mtxf { float m[4][4]; } Mtxf;
typedef struct Model { int unused; } Model;
struct StandTile { int unused; };
struct PadRecord { coord3d pos,look; struct StandTile *stan; };
typedef struct ChrRecord {
    u32 chrflags; s16 chrnum,padpreset1,chrpreset1; s8 headnum,bodynum;
    float hearingscale,visionrange;
} ChrRecord;
typedef struct PropRecord { ChrRecord *chr; coord3d pos; } PropRecord;
typedef struct GuardRecord {
    u16 chrnum,PadID,BodyID,AIListID,Preset,chrpreset1,health,ReactionTime,bitflags;
    s16 HeadID; ChrRecord *Data;
} GuardRecord;
static struct ChrModelFileRecord { int hasHead; } CitemZ_entries[2];
static struct PadRecord pads[1];
static struct { struct PadRecord *pads; } g_CurrentSetup={pads};
static ChrRecord chr;
static PropRecord prop={.chr=&chr};
static Model model;
static int activated,enabled;
static int getposstan(coord3d *pos,struct StandTile *stan,float radius,coord3d *out,struct StandTile **tile)
{ (void)radius; *out=*pos; *tile=stan; return 1; }
static int get_current_random_body(void) { return 1; }
static int bodyChooseHead(int body) { (void)body; return 1; }
static Model *retrieve_header_for_body_and_head(int body,int head,u32 flags)
{ (void)body; (void)head; (void)flags; return &model; }
static void *ailistFindById(int id) { (void)id; return NULL; }
static PropRecord *chrAllocate(Model *m,coord3d *pos,float angle,struct StandTile *tile,void *ai)
{ (void)m; (void)pos; (void)angle; (void)tile; (void)ai; return &prop; }
static void chrpropActivate(PropRecord *p) { assert(p==&prop); activated++; }
static void chrpropEnable(PropRecord *p) { assert(p==&prop); enabled++; }
static Mtxf view={.m={{1,0,0,0},{0,1,0,0},{0,0,1,0},{0,0,0,1}}};
static struct { float c_recipscaley; } player={100},*g_CurrentPlayer=&player;
static float g_PropFadeStartPx=16,g_PropFadeEndPx=13;
static Mtxf *camGetWorldToViewMtxf(void) { return &view; }
'''
source += function((ROOT / 'src/game/chraction.c').read_text(), 'expand_09_characters')
source += '\n'.join(re.findall(r'^#define CHRFADE_.*$', character, re.M)) + '\n'
source += function(character, 'chrCalcScreenFadeAlpha')
tick = function(character, 'chrTick')
cutoff = re.search(r'    if \(isOnScreen &&[^\n]*chrCalcScreenFadeAlpha[^\n]*\)\n    \{.*?\n    \}', tick, re.S)
assert cutoff, 'chrTick distance cutoff'
source += 'static int distanceCutoff(PropRecord *prop,int isOnScreen) {\nChrRecord *chr=prop->chr;\n'
source += cutoff[0] + '\nreturn isOnScreen;\n}\n'
source += r'''
int main(void)
{
    GuardRecord guard={.chrnum=1,.BodyID=1,.AIListID=0x413,.Preset=38,
        .chrpreset1=7,.health=1000,.ReactionTime=100,.HeadID=-1};
    const float pixels[]={17,16,14.5f,13,12,1};
    const int alphas[]={255,255,127,0,0,0};
    /* Every setup bit, alone and combined: only the three mapped flags change. */
    for(u32 bits=0;bits<=0xffff;bits++) {
        u32 expected=0x80000000;
        guard.bitflags=(u16)bits;
        chr.chrflags=expected;
        if(bits&GUARD_SETUP_FLAG_CHR_CLONE) expected|=CHRFLAG_CLONE;
        if(bits&GUARD_SETUP_FLAG_CHR_INVINCIBLE) expected|=CHRFLAG_INVINCIBLE;
        if(bits&GUARD_SETUP_FLAG_CHR_NOFADE) expected|=CHRFLAG_NOFADE;
        expand_09_characters(0,&guard,0);
        assert(chr.chrflags==expected && guard.Data==&chr);
        assert(chr.chrnum==1 && chr.padpreset1==38 && chr.chrpreset1==7);
        assert(chr.hearingscale==1 && chr.visionrange==100 && chr.bodynum==1);
    }
    assert(activated==65536 && enabled==65536);
    for(int exempt=0;exempt<2;exempt++) {
        guard.bitflags=exempt?GUARD_SETUP_FLAG_CHR_NOFADE:0;
        chr.chrflags=0;
        expand_09_characters(0,&guard,0);
        for(unsigned i=0;i<sizeof(pixels)/sizeof(*pixels);i++) {
            prop.pos.z=-20000.0f/pixels[i];
            assert(chrCalcScreenFadeAlpha(&prop)==alphas[i]);
            assert(distanceCutoff(&prop,1)==(exempt || alphas[i]>0));
            /* Offscreen/portal-rejected characters remain offscreen. */
            assert(distanceCutoff(&prop,0)==0);
        }
    }
    puts("PASS: saved character flags, 16px/13px fade thresholds, cutoff exemption and offscreen state.");
    return 0;
}
'''
with tempfile.TemporaryDirectory(prefix='gud-character-fade-') as folder:
    work = Path(folder)
    (work / 'check.c').write_text(source)
    subprocess.run([os.environ.get('CC', 'cc'), '-std=c99', '-O1', '-g', '-Wall', '-Wextra',
                    '-Werror', '-Wno-unused-parameter', '-fsanitize=address,undefined',
                    str(work / 'check.c'), '-lm', '-o', str(work / 'check')], check=True)
    subprocess.run([str(work / 'check')], check=True,
                   env=dict(os.environ, ASAN_OPTIONS='detect_leaks=0', UBSAN_OPTIONS='halt_on_error=1'))
