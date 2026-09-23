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
#include "actionblocks.h"

static const char *why, *dir;
static void Require(BOOL ok) { if (!ok) { fprintf(stderr,"%s\n",why); abort(); } }
static SetupFile Load(void)
{ SetupFile s={0}; Require(SetupLoadProjectFile(dir,"UsetupboundZ",&s,&why)); return s; }
static void GlobalPadReferences(SetupFile *s, BOOL references)
{
    unsigned char data[128]={0}; RomFile rom={.data=data,.size=sizeof(data)};
    rom.info.entrycount=2;
    rom.info.entries[0]=(RomManifestEntry){0x434d4150,0,128,0x80000000};
    rom.info.entries[1]=(RomManifestEntry){0x4149474c,16,32,0};
    SetupMetaWrite32(data+16,0x80000030); SetupMetaWrite32(data+20,1);
    SetupMetaWrite32(data+24,8); SetupMetaWrite32(data+28,1);
    SetupMetaWrite32(data+48,0x80000050); SetupMetaWrite32(data+52,2); data[80]=4;
    if (references) { memcpy(data+80,(unsigned char[]){0x1c,0x27,0x10,4},4); }
    Require(SetupFileSetGlobalReferences(s,&rom,&why));
}
static void GlobalReferences(SetupFile *s) { GlobalPadReferences(s,FALSE); }
static void Placement(const SetupFile *s, float scale, BOOL rotated, BOOL door)
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
    if (door)
    {
        expectedmin[0]=rotated ? 95 : 90; expectedmin[2]=rotated ? 270 : 295;
        /* The first triangle spans model X/Y. A native door must map those
         * edges to pad up/look, not ordinary prop side/up. Size alone would
         * miss the reported regression: the wrong face still fills the box. */
        const BgVertex *a=geometry.tris,*x=a+1,*y=a+2;
        assert(fabs((x->y-a->y)*scale-60)<.01);
        assert(fabs((x->x-a->x)*scale)<.01 && fabs((x->z-a->z)*scale)<.01);
        assert(fabs((y->y-a->y)*scale)<.01);
        assert(fabs((y->x-a->x)*scale-(rotated ? 20 : 0))<.01);
        assert(fabs((y->z-a->z)*scale-(rotated ? 0 : 20))<.01);
    }
    for (int a=0;a<3;a++)
    {
        assert(fabs((max[a]-min[a])*scale-dimensions[a])<.01);
        assert(fabs(min[a]*scale-expectedmin[a])<.01);
    }
    ObjectGeometryFree(&geometry);
}
static void Create(float scale, BOOL rotated, BOOL door)
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
    Require(SetupFileAddBoundPadModel(&s,0,1,door,&selected,&why));
    assert(selected==0 && s.objectcount==1 && s.padcount==0 && s.boundpadcount==1);
    assert(!memcmp(&original,&s.boundpads[0],sizeof(original)));
    assert(s.objects[0].type==(door ? PROPDEF_DOOR : PROPDEF_PROP) && s.objects[0].modelid==1);
    assert(s.objects[0].pad==(door ? 0 : 10000) && s.objects[0].extrascale==256);
    if (door)
    {
        SetupObjectProperties properties;
        Require(SetupFileGetObjectProperties(&s,selected,&properties,&why));
        assert(s.objects[0].flags==(PROPFLAG_FORCE_COLLISIONS|PROPFLAG_NO_PORTAL_CLOSE));
        assert(properties.door.type==DOORTYPE_SLIDING && properties.door.travel==1);
        assert(properties.door.accel>0 && properties.door.decel>0 && properties.door.speed>0);
        assert(SetupMetaRead32(s.data+s.objects[0].sourceoffset+0x100)==48);
    }
    Require(SetupFileGetModelPad(&s,selected,&linked)); assert(linked.bound && linked.index==0);
    Placement(&s,scale,rotated,door);
    Require(EditHistoryCommitEdit(&h,NULL,&s,NULL,&tx,&why));
    Require(SetupFileClone(&s,&after,&why));
    assert(!SetupFileAddBoundPadModel(&s,0,2,door,&selected,&why));
    SetupAssertNativeEqual(&s,&after);
    Require(EditHistoryUndo(&h,&bg,&s,&stan,&asset,&why)); SetupAssertNativeEqual(&s,&before);
    Require(EditHistoryRedo(&h,&bg,&s,&stan,&asset,&why)); SetupAssertNativeEqual(&s,&after);
    Require(SetupSaveProjectFile(dir,&s,&why)); Require(SetupLoadProjectFile(dir,s.name,&saved,&why));
    SetupAssertNativeEqual(&s,&saved); Placement(&saved,scale,rotated,door);
    /* Reusing a deleted object must not accumulate command records or pads. */
    DWORD compactsize=s.size;
    for (int i=0;i<20;i++)
    {
        Require(SetupFileDeleteObject(&s,0,&why));
        Require(SetupFileAddBoundPadModel(&s,0,1+i%2,door,&selected,&why));
        Require(SetupFileCompact(&s,&why));
        assert(selected==0 && s.objectcount==1 && s.boundpadcount==1 && s.size==compactsize);
    }
    SetupFileFree(&s); SetupFileFree(&before); SetupFileFree(&after); SetupFileFree(&saved); EditHistoryFree(&h);
}
static void Rejected(BOOL door)
{
    SetupFile s=Load(), before={0}; DWORD selected=999;
    Require(SetupFileClone(&s,&before,&why));
    assert(!SetupFileAddBoundPadModel(&s,1,1,door,&selected,&why));
    assert(!SetupFileAddBoundPadModel(&s,0,-1,door,&selected,&why));
    assert(!SetupFileAddBoundPadModel(&s,0,999,door,&selected,&why));
    s.boundpads[0].pad.occluder=TRUE;
    assert(!SetupFileAddBoundPadModel(&s,0,1,door,&selected,&why));
    s.boundpads[0].pad.occluder=FALSE; s.boundpads[0].pad.deleted=TRUE;
    assert(!SetupFileAddBoundPadModel(&s,0,1,door,&selected,&why));
    s.boundpads[0].pad.deleted=FALSE;
    assert(selected==999); SetupAssertNativeEqual(&s,&before);
    /* Door pad IDs are raw bound indices, even when their flag bits overlap
     * ordinary props' ownership bits. Test the parsed occupancy predicate. */
    SetupObject existingdoor={.type=PROPDEF_DOOR,.pad=0,.flags=PROPFLAG_ASSIGNEDTOCHR};
    s.objects=&existingdoor; s.objectcount=1;
    assert(!SetupFileCanAddBoundPadModel(&s,0,door,&why));
    s.objects=NULL; s.objectcount=0;
    SetupFileFree(&s); SetupFileFree(&before);
}
static void RecreateAsDoor(void)
{
    SetupFile s=Load(), saved={0}; DWORD prop,door;
    SetupBoundPad pad=s.boundpads[0];
    GlobalReferences(&s);
    Require(SetupFileAddBoundPadModel(&s,0,1,FALSE,&prop,&why));
    Require(SetupFileDeleteObject(&s,prop,&why));
    Require(SetupFileAddBoundPadModel(&s,0,1,TRUE,&door,&why));
    /* A 64-word door must not overwrite a 32-word prop tombstone. */
    assert(door!=prop && s.objects[prop].deleted && s.objects[door].type==PROPDEF_DOOR);
    assert(s.objects[door].pad==0 && s.boundpadcount==1 && !memcmp(&pad,s.boundpads,sizeof(pad)));
    Require(SetupSaveProjectFile(dir,&s,&why)); Require(SetupLoadProjectFile(dir,s.name,&saved,&why));
    SetupAssertNativeEqual(&s,&saved);
    SetupFileFree(&s); SetupFileFree(&saved);
}
/* Local/shared Action Blocks refer to bound pad 0 as 10000, while a native
 * door stores raw index 0. These references must follow the authored volume. */
static void LocalPadReference(SetupFile *s)
{
    DWORD at=(s->size+3u)&~3u, size=at+20;
    unsigned char *data=calloc(size,1); assert(data);
    memcpy(data,s->data,s->size); free(s->data); s->data=data; s->size=size;
    SetupMetaWrite32(data+20,at); SetupMetaWrite32(data+at,at+16); SetupMetaWrite32(data+at+4,0x401);
    memcpy(data+at+16,(unsigned char[]){0x1c,0x27,0x10,4},4);
}
static void CheckLocalPadReference(const SetupFile *s)
{
    ActionDocument actions={0};
    Require(ActionDocumentLoad(s,&actions,&why));
    assert(actions.count==1 && actions.blocks[0].id==0x401 && actions.blocks[0].count==2);
    assert(ActionReadValue(&actions.blocks[0].instructions[0],0)==10000);
    ActionDocumentFree(&actions);
}
static void ScriptedTransforms(BOOL door, int references, int mode)
{
    SetupFile s=Load(), before={0}, after={0}, saved={0};
    SetupObjectGeometry geometry={0}, transformed={0}, duplicate={0};
    EditHistory history={0}; EditHistoryTransaction tx={0}; EditHistoryAsset asset;
    BgDocument bg={0}; StanFile historyStan={0}; SetupPadRef ref; DWORD id,copy;
    const float levelscale=.21847887f;
    StanTile tile={.id=0x100,.sourceoffset=12,.room=1,.pointcount=3,.extreme={0,1,2},
        .points={{-10000,0,-10000,0},{-10000,0,30000,0},{30000,0,-10000,0}}};
    StanFile floor={.tiles=&tile,.tilecount=1,.levelscale=levelscale};
    const double offset[3]={10,20,30};
    double pivot[3]; Rotation rotation; Scaling scale={.factor={1.25,.75,1.5}};
    if (references) { GlobalPadReferences(&s,references==2); }
    if (references==1) { LocalPadReference(&s); CheckLocalPadReference(&s); }
    Require(SetupFileAddBoundPadModel(&s,0,1,door,&id,&why));
    Require(SetupFileCompact(&s,&why));
    Require(ObjectLoadSetupGeometry(dir,&s,&floor,levelscale,&geometry,&why));
    Require(SetupFileGetModelPad(&s,id,&ref));
    for (int a=0;a<3;a++) { pivot[a]=scale.pivot[a]=s.boundpads[0].pad.pos[a]/levelscale; }
    RotationAxis(&rotation,1,30); Require(SetupFilePadRotation(&s,&ref,&scale.axes));
    Require(SetupFileClone(&s,&before,&why)); EditHistoryReset(&history,NULL,&s,NULL);
    Require(EditHistoryBeginSetupEdit(&history,&s,"Transform scripted prop",&tx,&why));
    if (mode==0) { Require(ObjectTranslateSetupModel(dir,&s,&floor,levelscale,&geometry,id,offset,&transformed,&why)); }
    else if (mode==1) { Require(ObjectRotateSetupModel(dir,&s,&floor,levelscale,&geometry,id,&rotation,pivot,&transformed,&why)); }
    else { Require(ObjectScaleSetupModel(dir,&s,&floor,levelscale,&geometry,id,&scale,&transformed,&why)); }
    Require(SetupFileGetModelPad(&s,id,&ref));
    assert(ref.bound && ref.index==0 && s.boundpadcount==1 && s.objects[id].pad==(door ? 0 : 10000));
    assert(memcmp(s.boundpads,before.boundpads,sizeof(*s.boundpads)));
    Require(EditHistoryCommitEdit(&history,NULL,&s,NULL,&tx,&why));
    Require(SetupFileClone(&s,&after,&why));
    Require(EditHistoryUndo(&history,&bg,&s,&historyStan,&asset,&why)); SetupAssertNativeEqual(&s,&before);
    Require(EditHistoryRedo(&history,&bg,&s,&historyStan,&asset,&why)); SetupAssertNativeEqual(&s,&after);
    Require(SetupSaveProjectFile(dir,&s,&why)); Require(SetupLoadProjectFile(dir,s.name,&saved,&why));
    SetupAssertNativeEqual(&s,&saved);
    if (references==1) { CheckLocalPadReference(&saved); }
    /* Repeated moves must not accumulate pads or obsolete table copies. */
    DWORD size=s.size;
    for (int i=0;i<40;i++)
    {
        SetupObjectGeometry next={0};
        const double step[3]={i%2 ? -1 : 1,0,0};
        Require(ObjectTranslateSetupModel(dir,&s,&floor,levelscale,&transformed,id,step,&next,&why));
        Require(SetupFileCompact(&s,&why));
        assert(s.boundpadcount==1 && s.objects[id].pad==(door ? 0 : 10000) && s.size==size);
        ObjectGeometryFree(&transformed); transformed=next;
    }
    /* Explicit duplication still gets its own pad and leaves the scripted
     * original untouched, even without a shared-script catalog. */
    SetupBoundPad original=s.boundpads[0];
    Require(ObjectDuplicateSetupModel(dir,&s,&s,&floor,levelscale,&transformed,id,offset,
        NULL,NULL,NULL,&copy,&duplicate,&why));
    Require(SetupFileGetModelPad(&s,copy,&ref));
    assert(ref.bound && ref.index!=0 && s.boundpadcount==2);
    assert(!memcmp(&original,s.boundpads,sizeof(original)));
    if (references==1) { CheckLocalPadReference(&s); }
    if (references)
    {
        /* The looser transform rule must not allow deletion/recycling of a
         * pad still used by a level or shared Action Block. */
        SetupPadRef originalref={.bound=TRUE,.index=0};
        Require(SetupFileDeleteObject(&s,id,&why));
        assert(!SetupFileDeletePad(&s,&originalref,NULL,&why));
        assert(strstr(why,"Action Block"));
    }
    ObjectGeometryFree(&geometry); ObjectGeometryFree(&transformed); ObjectGeometryFree(&duplicate);
    SetupFileFree(&s); SetupFileFree(&before); SetupFileFree(&after); SetupFileFree(&saved); EditHistoryFree(&history);
}
static void SharedPlacement(BOOL door)
{
    SetupFile s=Load(); DWORD id,copy; SetupPadRef ref;
    const double offset[3]={1,0,0};
    GlobalReferences(&s);
    Require(SetupFileAddBoundPadModel(&s,0,1,door,&id,&why));
    Require(SetupFileDuplicateObject(&s,&s,id,&copy,&why));
    /* Author a genuinely shared placement, then move just one object. */
    s.objects[copy].pad=s.objects[id].pad;
    SetupMetaWrite32(s.data+s.objects[copy].sourceoffset+4,
        ((DWORD)s.objects[copy].modelid<<16)|(unsigned short)s.objects[copy].pad);
    SetupBoundPad original=s.boundpads[0];
    Require(SetupFileTranslateModel(&s,id,1,offset,&why));
    Require(SetupFileGetModelPad(&s,id,&ref));
    assert(ref.bound && ref.index!=0 && !memcmp(&original,s.boundpads,sizeof(original)));
    Require(SetupFileGetModelPad(&s,copy,&ref)); assert(ref.bound && ref.index==0);
    SetupFileFree(&s);
}
int main(int argc,char **argv)
{
    assert(argc==2); dir=argv[1]; SetupFile fixture=Load();
    Rejected(FALSE); Rejected(TRUE);
    RecreateAsDoor(); Require(SetupSaveProjectFile(dir,&fixture,&why));
    for (int door=0;door<2;door++) for (int rotated=0;rotated<2;rotated++) for (int scaled=0;scaled<2;scaled++)
    {
        Create(scaled ? .21847887f : 1,rotated,door);
        Require(SetupSaveProjectFile(dir,&fixture,&why));
    }
    for (int door=0;door<2;door++) for (int references=0;references<3;references++) for (int mode=0;mode<3;mode++)
    {
        ScriptedTransforms(door,references,mode);
        Require(SetupSaveProjectFile(dir,&fixture,&why));
    }
    SharedPlacement(FALSE); SharedPlacement(TRUE);
    SetupFileFree(&fixture);
    puts("PASS scripted bound-pad transforms: local/shared/unknown references, move/rotate/scale, retained IDs, undo/redo, save/reload, repeated edits, independent duplicates and deletion protection.");
    puts("PASS bound-pad props and doors: native fitting/orientation at two level scales, retained pad references, save/reload, undo/redo, tombstone reuse, occupied/invalid/occluder rejection.");
    return 0;
}
