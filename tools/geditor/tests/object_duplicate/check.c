#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <src/propconstants.h>
#include "objectload.h"
#include "setup_compare.h"
#include "setupmeta.h"
#include "bghistory.h"

static const char *why, *dir;
static void Require(BOOL ok) { if(!ok) { fprintf(stderr,"%s\n",why); abort(); } }
static SetupFile Load(void)
{ SetupFile s={0}; Require(SetupLoadProjectFile(dir,"UsetupduplicateZ",&s,&why)); return s; }
static void Near(double a,double b) { assert(fabs(a-b)<.025); }
static void RoundTrip(SetupFile *s)
{
    SetupFile saved={0}; Require(SetupFileCompact(s,&why));
    Require(SetupSaveProjectFile(dir,s,&why)); Require(SetupLoadProjectFile(dir,s->name,&saved,&why));
    SetupAssertNativeEqual(s,&saved); SetupFileFree(&saved);
}
static void Records(void)
{
    SetupFile original=Load(), s={0};
    Require(SetupFileClone(&original,&s,&why));
    for(DWORD i=0;i<original.objectcount;i++)
    {
        DWORD selected, at=original.objects[i].sourceoffset;
        DWORD bytes=(i+1<original.objectcount ? original.objects[i+1].sourceoffset : original.size-4)-at;
        SetupPadRef from,to;
        Require(SetupFileDuplicateObject(&s,&original,i,&selected,&why));
        assert(selected==original.objectcount+i && s.objects[selected].type==original.objects[i].type);
        Require(SetupFileGetModelPad(&original,i,&from)); Require(SetupFileGetModelPad(&s,selected,&to));
        assert(from.bound==to.bound && from.index!=to.index);
        unsigned char kind=original.objects[i].type;
        for(DWORD k=0;k<bytes;k++)
        {
            if(k==6 || k==7 || (kind==PROPDEF_DOOR && k>=128 && k<132)
                || ((kind==PROPDEF_CCTV || kind==PROPDEF_AUTOGUN) && k>=128 && k<132)
                || (kind==PROPDEF_MONITOR && k>=244 && k<248)) { continue; }
            assert(s.data[s.objects[selected].sourceoffset+k]==original.data[at+k]);
        }
        if(kind==PROPDEF_CCTV || kind==PROPDEF_AUTOGUN)
        {
            DWORD aim=SetupMetaRead32(s.data+s.objects[selected].sourceoffset+128);
            assert(aim!=to.index && aim!=0 && aim<s.padcount);
            assert(!memcmp(s.pads[aim].pos,original.pads[0].pos,sizeof(s.pads[aim].pos)));
        }
        assert(!memcmp(s.boundpads[0].pad.pos,original.boundpads[0].pad.pos,sizeof(original.pads[0].pos)));
    }
    assert(!SetupFileCanDuplicateObject(&s,SETUP_CHARACTER_SELECTION_BIT));
    Require(SetupFileDeleteObject(&s,1,&why)); assert(!SetupFileCanDuplicateObject(&s,1));
    RoundTrip(&s); SetupFileFree(&s); SetupFileFree(&original);
    puts("PASS: all 21 native prop types retain complete records, independent pads and aim targets; save/reload.");
}
static void FailuresAndEmpty(void)
{
    SetupFile s=Load(), source={0}, baseline={0}, empty={0}; DWORD selected=999;
    Require(SetupFileClone(&s,&source,&why)); Require(SetupFileClone(&s,&baseline,&why));
    /* A bad native target is detected after pad allocation; no partial copy
     * may escape the native operation. */
    SetupMetaWrite32(source.data+source.objects[4].sourceoffset+128,999999);
    assert(!SetupFileDuplicateObject(&s,&source,4,&selected,&why));
    assert(selected==999); SetupAssertNativeEqual(&s,&baseline);
    assert(!SetupFileDuplicateObject(&empty,&source,1,&selected,&why));
    Require(SetupFileClone(&baseline,&empty,&why));
    SetupMetaWrite32(empty.data+12,0);
    free(empty.objects); empty.objects=NULL; empty.objectcount=0;
    Require(SetupFileDuplicateObject(&empty,&source,1,&selected,&why));
    assert(selected==0 && empty.objectcount==1);
    SetupObjectGeometry before={0}, out={0};
    EditHistory h={0}; EditHistoryTransaction tx={0};
    Require(ObjectLoadSetupGeometry(dir,&s,NULL,1,&before,&why));
    EditHistoryReset(&h,NULL,&s,NULL);
    Require(EditHistoryBeginSetupEdit(&h,&s,"Duplicate Object",&tx,&why));
    const double invalid[3]={NAN,0,0};
    assert(!ObjectDuplicateSetupModel(dir,&s,&s,NULL,1,&before,1,invalid,NULL,NULL,NULL,&selected,&out,&why));
    EditHistoryRollbackEdit(&tx,NULL,&s,NULL); EditHistoryCancelEdit(&tx);
    SetupAssertNativeEqual(&s,&baseline); assert(!h.undocount && !out.tricount);
    ObjectGeometryFree(&before); ObjectGeometryFree(&out); EditHistoryFree(&h);
    SetupFileFree(&s); SetupFileFree(&source); SetupFileFree(&baseline); SetupFileFree(&empty);
    puts("PASS: empty command lists, atomic native failure and failed-transform transaction rollback.");
}
static void Transform(int mode,float levelscale,BOOL bounded,DWORD original,BOOL live)
{
    SetupFile s=Load(), snapshot={0}, baseline={0}, after={0};
    SetupObjectGeometry before={0}, clipboard={0}, objects={0};
    EditHistory h={0}; EditHistoryTransaction tx={0}; EditHistoryAsset asset;
    BgDocument bg={0}; StanFile stan={0};
    /* A real stan floor catches accidental re-grounding or stacking onto
     * the original when a copy is supposed to stay exactly 10 units above. */
    StanTile tile={.id=0,.room=1,.pointcount=3,.extreme={0,1,2},
        .points={{-1000,0,-1000,0},{0,0,2000,0},{2000,0,-1000,0}}};
    StanFile floor={.tiles=&tile,.tilecount=1,.levelscale=levelscale};
    DWORD selected;
    const double bounds[6]={-4,6,-8,12,-3,7}, offset[3]={0,10,0}, pivot[3]={10,20,30};
    Rotation rotation; RotationAxis(&rotation,1,35);
    Scaling scale={.factor={1.5,1,1}}; RotationAxis(&scale.axes,0,0); memcpy(scale.pivot,pivot,sizeof(pivot));
    if(bounded) { Require(SetupFileSetModelBounds(&s,original,levelscale,bounds,&why)); }
    SetupPadRef scalingref;
    Require(SetupFileGetModelPad(&s,original,&scalingref));
    Require(SetupFilePadRotation(&s,&scalingref,&scale.axes));
    Require(ObjectLoadSetupGeometry(dir,&s,&floor,levelscale,&before,&why));
    Require(ObjectCopySetupModelPose(&before,original,&clipboard,&why));
    Require(SetupFileClone(&s,&snapshot,&why));
    /* Copy is a snapshot: moving/deleting the source cannot alter its pose. */
    const double moved[3]={100,-20,40};
    if(!live)
    {
        Require(SetupFileTranslateModel(&s,original,levelscale,moved,&why));
        Require(SetupFileDeleteObject(&s,original,&why));
    }
    Require(SetupFileClone(&s,&baseline,&why));
    EditHistoryReset(&h,NULL,&s,NULL);
    Require(EditHistoryBeginSetupEdit(&h,&s,"Duplicate Object",&tx,&why));
    Require(ObjectDuplicateSetupModel(dir,&s,live ? &s : &snapshot,&floor,levelscale,live ? &before : &clipboard,original,
        mode==0 ? offset : NULL,mode==1 ? &rotation : NULL,mode==1 ? pivot : NULL,
        mode==2 ? &scale : NULL,&selected,&objects,&why));
    DWORD j=0;
    for(DWORD i=0;i<objects.tricount;i++) if(objects.objectindices[i]==selected)
    {
        for(int corner=0;corner<3;corner++)
        {
            const BgVertex *v=&clipboard.tris[j*3+corner], *w=&objects.tris[i*3+corner];
            double point[3]={v->x,v->y,v->z}, expected[3];
            if(mode==1) { RotationPoint(&rotation,pivot,point,expected); }
            else if(mode==2) { ScalingPoint(&scale,point,expected); }
            else for(int a=0;a<3;a++) { expected[a]=point[a]+offset[a]; }
            Near(w->x,expected[0]); Near(w->y,expected[1]); Near(w->z,expected[2]);
        }
        j++;
    }
    assert(j==clipboard.tricount);
    SetupObjectProperties oldprops, newprops;
    Require(SetupFileGetObjectProperties(&snapshot,original,&oldprops,&why));
    Require(SetupFileGetObjectProperties(&s,selected,&newprops,&why));
    if(oldprops.object.type==PROPDEF_CCTV || oldprops.object.type==PROPDEF_AUTOGUN)
    {
        LONG a=oldprops.object.type==PROPDEF_CCTV ? oldprops.cctv.lookpad : oldprops.drone.aimpad;
        LONG b=oldprops.object.type==PROPDEF_CCTV ? newprops.cctv.lookpad : newprops.drone.aimpad;
        double point[3], expected[3];
        assert(a!=b);
        for(int axis=0;axis<3;axis++) { point[axis]=snapshot.pads[a].pos[axis]/levelscale; }
        if(mode==1) { RotationPoint(&rotation,pivot,point,expected); }
        else if(mode==2) { ScalingPoint(&scale,point,expected); }
        else for(int axis=0;axis<3;axis++) { expected[axis]=point[axis]+offset[axis]; }
        for(int axis=0;axis<3;axis++) { Near(s.pads[b].pos[axis]/levelscale,expected[axis]); }
        assert(!memcmp(s.pads[a].pos,snapshot.pads[a].pos,sizeof(s.pads[a].pos)));
    }
    Require(EditHistoryCommitEdit(&h,NULL,&s,NULL,&tx,&why)); assert(h.undocount==1);
    Require(SetupFileClone(&s,&after,&why));
    Require(EditHistoryUndo(&h,&bg,&s,&stan,&asset,&why));
    SetupAssertNativeEqual(&s,&baseline);
    Require(EditHistoryRedo(&h,&bg,&s,&stan,&asset,&why)); SetupAssertNativeEqual(&s,&after);
    RoundTrip(&s);
    ObjectGeometryFree(&before); ObjectGeometryFree(&clipboard); ObjectGeometryFree(&objects);
    SetupFileFree(&snapshot); SetupFileFree(&baseline); SetupFileFree(&after); SetupFileFree(&s); EditHistoryFree(&h);
}
int main(int argc,char **argv)
{
    assert(argc==2); dir=argv[1];
    /* Keep the fixture on disk for each independent case. */
    SetupFile fixture=Load();
    FailuresAndEmpty();
    Records(); Require(SetupSaveProjectFile(dir,&fixture,&why));
    for(int mode=0;mode<3;mode++) for(int bound=0;bound<2;bound++)
        for(int scale=0;scale<2;scale++)
        { Transform(mode,scale ? .53931433f : 1,bound,1,FALSE); Require(SetupSaveProjectFile(dir,&fixture,&why)); }
    /* Live source aliases the destination during Shift-drag; include aimed
     * devices and doors as well as an ordinary prop. */
    const DWORD props[]={0,1,4,10};
    for(int mode=0;mode<3;mode++) for(unsigned i=0;i<sizeof(props)/sizeof(*props);i++)
    { Transform(mode,1,FALSE,props[i],TRUE); Require(SetupSaveProjectFile(dir,&fixture,&why)); }
    SetupFileFree(&fixture);
    puts("PASS: copied visible pose survives source move/delete; +10 paste, rotation and scaling, normal/bound pads and level scales; single-action undo/redo.");
    return 0;
}
