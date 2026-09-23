#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <src/propconstants.h>
#include "objectload.h"
#include "bghistory.h"
#include "setupmeta.h"
#include "setup_compare.h"

static const char *why, *dir;
static void Require(BOOL ok) { if (!ok) { fprintf(stderr,"%s\n",why); abort(); } }
static SetupFile Load(void)
{ SetupFile s={0}; Require(SetupLoadProjectFile(dir,"UsetupboundZ",&s,&why)); return s; }
static void GlobalReferences(SetupFile *s)
{
    unsigned char data[128]={0}; RomFile rom={.data=data,.size=sizeof(data)};
    rom.info.entrycount=2;
    rom.info.entries[0]=(RomManifestEntry){0x434d4150,0,128,0x80000000};
    rom.info.entries[1]=(RomManifestEntry){0x4149474c,16,32,0};
    SetupMetaWrite32(data+16,0x80000030); SetupMetaWrite32(data+20,1);
    SetupMetaWrite32(data+24,8); SetupMetaWrite32(data+28,1);
    SetupMetaWrite32(data+48,0x80000050); SetupMetaWrite32(data+52,2); data[80]=4;
    Require(SetupFileSetGlobalReferences(s,&rom,&why));
}
static void Placement(const SetupFile *s, float scale, BOOL rotated)
{
    SetupObjectGeometry geometry={0};
    double min[3]={1e9,1e9,1e9}, max[3]={-1e9,-1e9,-1e9};
    Require(ObjectLoadSetupGeometry(dir,s,NULL,scale,&geometry,&why));
    assert(geometry.tricount==4);
    for (DWORD i=0;i<geometry.tricount*3;i++)
    {
        const BgVertex *v=geometry.tris+i;
        const double p[3]={v->x,v->y,v->z};
        assert(geometry.objectindices[i/3]==0);
        for (int a=0;a<3;a++) { min[a]=fmin(min[a],p[a]); max[a]=fmax(max[a],p[a]); }
    }
    /* The asymmetric fixture's model origin is not its bbox centre. Native
     * props retain that origin in X/Z, while anchoring their bottom to pad Y. */
    double expectedmin[3]={rotated ? 99 : 94,180,rotated ? 266 : 299};
    double dimensions[3]={rotated ? 20 : 40,60,rotated ? 40 : 20};
    for (int a=0;a<3;a++)
    {
        assert(fabs((max[a]-min[a])*scale-dimensions[a])<.01);
        assert(fabs(min[a]*scale-expectedmin[a])<.01);
    }
    ObjectGeometryFree(&geometry);
}
static void Create(float scale, BOOL rotated)
{
    SetupFile s=Load(), before={0}, after={0}, saved={0};
    SetupPadRef pad={.bound=TRUE,.index=0}, linked;
    EditHistory h={0}; EditHistoryTransaction tx={0}; EditHistoryAsset asset;
    BgDocument bg={0}; StanFile stan={0}; DWORD selected=999;
    GlobalReferences(&s);
    if (rotated)
    {
        Rotation rotation; BOOL changed;
        RotationAxis(&rotation,1,90);
        Require(SetupFileRotatePad(&s,&pad,&rotation,&changed,&why));
    }
    SetupBoundPad original=s.boundpads[0];
    Require(SetupFileClone(&s,&before,&why)); EditHistoryReset(&h,NULL,&s,NULL);
    Require(EditHistoryBeginSetupEdit(&h,&s,"Create Object on Bound Pad",&tx,&why));
    Require(SetupFileAddBoundPadModel(&s,0,1,&selected,&why));
    assert(selected==0 && s.objectcount==1 && s.padcount==0 && s.boundpadcount==1);
    assert(!memcmp(&original,&s.boundpads[0],sizeof(original)));
    assert(s.objects[0].type==PROPDEF_PROP && s.objects[0].modelid==1);
    assert(s.objects[0].pad==10000 && s.objects[0].extrascale==256);
    Require(SetupFileGetModelPad(&s,selected,&linked)); assert(linked.bound && linked.index==0);
    Placement(&s,scale,rotated);
    Require(EditHistoryCommitEdit(&h,NULL,&s,NULL,&tx,&why));
    Require(SetupFileClone(&s,&after,&why));
    assert(!SetupFileAddBoundPadModel(&s,0,2,&selected,&why));
    SetupAssertNativeEqual(&s,&after);
    Require(EditHistoryUndo(&h,&bg,&s,&stan,&asset,&why)); SetupAssertNativeEqual(&s,&before);
    Require(EditHistoryRedo(&h,&bg,&s,&stan,&asset,&why)); SetupAssertNativeEqual(&s,&after);
    Require(SetupSaveProjectFile(dir,&s,&why)); Require(SetupLoadProjectFile(dir,s.name,&saved,&why));
    SetupAssertNativeEqual(&s,&saved); Placement(&saved,scale,rotated);
    /* Reusing a deleted object must not accumulate command records or pads. */
    DWORD compactsize=s.size;
    for (int i=0;i<20;i++)
    {
        Require(SetupFileDeleteObject(&s,0,&why));
        Require(SetupFileAddBoundPadModel(&s,0,1+i%2,&selected,&why));
        Require(SetupFileCompact(&s,&why));
        assert(selected==0 && s.objectcount==1 && s.boundpadcount==1 && s.size==compactsize);
    }
    SetupFileFree(&s); SetupFileFree(&before); SetupFileFree(&after); SetupFileFree(&saved); EditHistoryFree(&h);
}
static void Rejected(void)
{
    SetupFile s=Load(), before={0}; DWORD selected=999;
    Require(SetupFileClone(&s,&before,&why));
    assert(!SetupFileAddBoundPadModel(&s,1,1,&selected,&why));
    assert(!SetupFileAddBoundPadModel(&s,0,-1,&selected,&why));
    assert(!SetupFileAddBoundPadModel(&s,0,999,&selected,&why));
    s.boundpads[0].pad.occluder=TRUE;
    assert(!SetupFileAddBoundPadModel(&s,0,1,&selected,&why));
    s.boundpads[0].pad.occluder=FALSE; s.boundpads[0].pad.deleted=TRUE;
    assert(!SetupFileAddBoundPadModel(&s,0,1,&selected,&why));
    s.boundpads[0].pad.deleted=FALSE;
    assert(selected==999); SetupAssertNativeEqual(&s,&before);
    /* Door pad IDs are raw bound indices, even when their flag bits overlap
     * ordinary props' ownership bits. Test the parsed occupancy predicate. */
    SetupObject door={.type=PROPDEF_DOOR,.pad=0,.flags=PROPFLAG_ASSIGNEDTOCHR};
    s.objects=&door; s.objectcount=1;
    assert(!SetupFileCanAddBoundPadModel(&s,0,&why));
    s.objects=NULL; s.objectcount=0;
    SetupFileFree(&s); SetupFileFree(&before);
}
int main(int argc,char **argv)
{
    assert(argc==2); dir=argv[1]; SetupFile fixture=Load();
    Rejected();
    for (int rotated=0;rotated<2;rotated++) for (int scaled=0;scaled<2;scaled++)
    {
        Create(scaled ? .21847887f : 1,rotated);
        Require(SetupSaveProjectFile(dir,&fixture,&why));
    }
    SetupFileFree(&fixture);
    puts("PASS bound-pad models: native fitting/orientation at two level scales, retained pad references, save/reload, undo/redo, tombstone reuse, occupied/invalid/occluder rejection.");
    return 0;
}
