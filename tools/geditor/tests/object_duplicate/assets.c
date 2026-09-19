/* Replace only asset/ROM IO. Production placement, transforms, setup parsing,
 * compaction and edit history run against a deterministic asymmetric mesh. */
#include <assert.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>
#include "modelload.h"
#include "modeledits.h"
#include "characterload.h"
#include "bghistory.h"
#include "character-placement.inc"

BOOL RomLoad(const char *path, RomFile *rom, const char **why) { memset(rom,0,sizeof(*rom)); return TRUE; }
void RomFree(RomFile *rom) {}
BOOL RomFindFile(const RomFile *rom,const char *name,DWORD *offset,DWORD *size,const char **why) { return FALSE; }
BOOL ModelGetPropDefinition(int id,const char **name,float *scale) { return FALSE; }
BOOL ModelReadPlacementBounds(const unsigned char *data,DWORD size,float min[3],float max[3]) { abort(); }
BOOL ModelReadMonitorScreen(const unsigned char *data,DWORD size,int index,BgVertex quad[4]) { abort(); }
BOOL ModelReadSwitchAttachment(const unsigned char *data,DWORD size,int switchindex,int childindex,float position[3]) { abort(); }
const unsigned char *ModelEditsGetData(const char *dir,const char *name,DWORD *size,const char **why) { abort(); }
BOOL MonitorBankLoadRom(MonitorBank *out,const RomFile *rom,const char **why) { abort(); }
void MonitorGeometryFree(MonitorGeometry *geometry) { assert(!geometry->count); }
BOOL CharacterLoadSetupGeometry(const char *dir,const SetupFile *setup,const StanFile *stan,
    const RomFile *rom,float scale,SetupObjectGeometry *out,const char **why)
{
    /* Replace skeletal asset IO with an asymmetric armed pose; use production
     * character grounding and weapon ownership so copies must retain both. */
    memset(out,0,sizeof(*out));
    DWORD capacity=setup->charactercount*3;
    if(!capacity)return TRUE;
    out->tris=calloc(capacity*3,sizeof(*out->tris));out->objectindices=calloc(capacity,sizeof(*out->objectindices));
    out->tritags=calloc(capacity,sizeof(*out->tritags));out->renderflags=calloc(capacity,sizeof(*out->renderflags));
    out->occupiedpads=calloc(setup->padcount,1);
    assert(out->tris && out->objectindices && out->tritags && out->renderflags && out->occupiedpads);
    for(DWORD i=0;i<setup->charactercount;i++) {
        const SetupCharacter *chr=setup->characters+i;float pos[3];const SetupObject *held[2];
        if(chr->deleted || chr->pad>=setup->padcount
            || !CharacterGetPadPosition(setup->pads+chr->pad,stan,scale,pos))continue;
        SetupFileGetCharacterHeldWeapons(setup,i,held);
        for(int part=0;part<3;part++) {
            if(part && !held[part-1])continue;
            DWORD n=out->tricount++;out->objectindices[n]=SETUP_CHARACTER_SELECTION_BIT|i;
            for(int c=0;c<3;c++) {
                BgVertex *v=out->tris+n*3+c;
                v->x=pos[0]+(part ? 40*part : -5)+c;
                v->y=pos[1]+(c ? 100 : 0);v->z=pos[2]+10*c;
            }
        }
        out->occupiedpads[chr->pad]=1;out->objectcount++;
    }
    return TRUE;
}
void BgDocumentFree(BgDocument *document) { memset(document,0,sizeof(*document)); }
void StanFileFree(StanFile *stan) { memset(stan,0,sizeof(*stan)); }
BgVertex *ModelLoadProjectGeometry(const char *dir,int id,DWORD *count,unsigned short **tags,
    BgRenderFlags **flags,float *scale,const char **why)
{
    static const float corners[4][3]={{-4,-8,-3},{6,-8,-3},{-4,12,-3},{-4,-8,7}};
    static const int triangles[12]={0,1,2,0,3,1,0,2,3,1,3,2};
    BgVertex *vertices=calloc(12,sizeof(*vertices));
    *count=4; *scale=1; *tags=calloc(4,sizeof(**tags)); *flags=calloc(4,sizeof(**flags));
    assert(vertices && *tags && *flags);
    for(int i=0;i<12;i++)
    { vertices[i].x=corners[triangles[i]][0]; vertices[i].y=corners[triangles[i]][1]; vertices[i].z=corners[triangles[i]][2]; }
    return vertices;
}
