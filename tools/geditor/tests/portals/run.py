#!/usr/bin/env python3
"""Portal BG persistence and production viewport picking under ASan/UBSan."""
import os
from pathlib import Path
import re
import subprocess
import tempfile


def function(source, name):
    match = re.search(r'^(?:static )?(?:BOOL|DWORD|void|double|f32) '+name+r'\([^;]*?\)\s*\{', source, re.M)
    start = source.index('{', match.start())
    depth, end = 1, start + 1
    while depth:
        depth += (source[end] == '{') - (source[end] == '}')
        end += 1
    return source[match.start():end]


def main():
    here = Path(__file__).resolve().parent
    src = here.parents[1] / 'src'
    shim = here.parent / 'image_import'
    with tempfile.TemporaryDirectory(prefix='geditor-portals-') as temp:
        work = Path(temp)
        flags = [os.environ.get('CC', 'cc'), '-O1', '-g', '-std=c99', '-Wall', '-Wextra',
                 '-ffunction-sections', '-fdata-sections', '-fsanitize=address,undefined',
                 f'-I{shim}', f'-I{src}', '-Wl,--gc-sections']
        env = dict(os.environ, ASAN_OPTIONS='detect_leaks=0', UBSAN_OPTIONS='halt_on_error=1')
        sources = ['bgload.c', 'bgdocument.c', 'bgcompile.c', 'bgmaterial.c', 'bgrender.c', 'bghistory.c']
        subprocess.run(flags + [str(here/'check.c'), str(shim/'platform.c')]
                       + [str(src/name) for name in sources] + ['-lm', '-o', str(work/'check')], check=True)
        subprocess.run([str(work/'check'), str(work)], check=True, env=env)
        viewport = (src/'viewport.c').read_text()
        prelude = '''#include <assert.h>\n#include <float.h>\n#include <math.h>\n#include <stdio.h>\n#include "bgload.h"
            typedef struct { float x,y,z; unsigned char r,g,b,a; float s,t; } Vertex;
            typedef struct { double origin[3],direction[3],mindistance,maxdistance; } ViewportPickRay;
            typedef struct { BOOL showportals; BgPortalFile portals; DWORD selectedportal; Vertex *portalfill,*portaledges; } ViewportState;
        '''
        prelude += '\n'.join(line for line in viewport.splitlines() if line.startswith(('#define VIEWPORT_PICK_', '#define VIEWPORT_SELECTION_GOLD')))+'\n'
        for name in ['ViewportRayTriangleDistance', 'ViewportCoplanarPickTolerance', 'ViewportFindPickedPortal', 'ViewportPortalGeometryIsFirst', 'ViewportRefreshPortalColors']:
            prelude += function(viewport, name)+'\n'
        # Use the game's actual margin decoder as the reference for all byte values.
        game = (src.parents[2]/'src/game/bg.c').read_text()
        prelude += 'typedef float f32; typedef int s32; static BgPortal g_BgPortals[1];\n'
        prelude += function(game, 'bgGetPortalMargin')+'\n'
        tests = r'''
        int main(void) {
            BgPortal portals[3]={0};Vertex fill[12]={0},edges[16]={0};
            ViewportState state={TRUE,{portals,3},BG_PORTAL_INDEX_NONE,fill,edges};
            ViewportPickRay ray={{0,0,0},{0,0,1},.01,100};
            const float xy[4][2]={{-5,-5},{5,-5},{5,5},{-5,5}};
            for (int i=0;i<3;i++) {
                portals[i].pointcount=4;portals[i].geometryoffset=i==2?2:1;
                for(int v=0;v<4;v++) { portals[i].points[v]=(BgPortalPoint){xy[v][0],xy[v][1],i==2?20:10}; }
            }
            assert(ViewportFindPickedPortal(&state,&ray,DBL_MAX,FALSE)==0);
            state.selectedportal=0;assert(ViewportFindPickedPortal(&state,&ray,DBL_MAX,FALSE)==1);
            assert(ViewportFindPickedPortal(&state,&ray,DBL_MAX,TRUE)==0);
            state.selectedportal=1;assert(ViewportFindPickedPortal(&state,&ray,DBL_MAX,FALSE)==0);
            assert(ViewportFindPickedPortal(&state,&ray,5,FALSE)==BG_PORTAL_INDEX_NONE); /* blocked by visible geometry */
            assert(ViewportFindPickedPortal(&state,&ray,10,FALSE)==0); /* coplanar BG */
            state.showportals=FALSE;assert(ViewportFindPickedPortal(&state,&ray,DBL_MAX,FALSE)==BG_PORTAL_INDEX_NONE);
            state.showportals=TRUE;ray.origin[2]=30;ray.direction[2]=-1;state.selectedportal=BG_PORTAL_INDEX_NONE;
            assert(ViewportFindPickedPortal(&state,&ray,DBL_MAX,FALSE)==2); /* back side */
            ray.origin[0]=100;assert(ViewportFindPickedPortal(&state,&ray,DBL_MAX,FALSE)==BG_PORTAL_INDEX_NONE);
            ray.origin[0]=0;ray.maxdistance=5;assert(ViewportFindPickedPortal(&state,&ray,DBL_MAX,FALSE)==BG_PORTAL_INDEX_NONE);
            state.selectedportal=1;ViewportRefreshPortalColors(&state);
            for(int i=0;i<12;i++) { assert(fill[i].r==(i<6?255:0) && fill[i].g==(i<6?210:255) && fill[i].b==(i<6?0:255)); }
            for(int i=0;i<16;i++) { assert(edges[i].r==(i<8?255:0) && edges[i].g==(i<8?210:255) && edges[i].b==(i<8?0:255)); }
            state.selectedportal=BG_PORTAL_INDEX_NONE;ViewportRefreshPortalColors(&state);
            for(int i=0;i<12;i++) { assert(fill[i].r==0 && fill[i].g==255 && fill[i].b==255); }
            for(int value=0;value<256;value++) { g_BgPortals[0].controlbytes2=value;
                assert(BgPortalGetMargin(&g_BgPortals[0])==bgGetPortalMargin(0)); }
            puts("PASS: visible/double-sided portal picking, occlusion, coincident-entry cycling, Ctrl-click targeting, clipping, shared-polygon highlight colors and all native margin encodings.");
        }
        '''
        (work/'pick.c').write_text(prelude+tests)
        subprocess.run(flags+[str(work/'pick.c'), str(src/'bgload.c'), '-lm', '-o', str(work/'pick')], check=True)
        subprocess.run([str(work/'pick')], check=True, env=env)


if __name__ == '__main__':
    main()
