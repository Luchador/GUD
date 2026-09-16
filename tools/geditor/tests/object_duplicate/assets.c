/* Replace only asset/ROM IO. Production placement, transforms, setup parsing,
 * compaction and edit history run against a deterministic asymmetric mesh. */
#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include "modelload.h"
#include "modeledits.h"
#include "characterload.h"
#include "bghistory.h"

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
{ memset(out,0,sizeof(*out)); return TRUE; }
BOOL CharacterGetPadPosition(const SetupPad *pad,const StanFile *stan,float scale,float pos[3]) { abort(); }
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
