#include <assert.h>
#include <float.h>
#include <math.h>
#include <stdio.h>
#include <string.h>
#include "objectload.h"
#include "bghistory.h"
#include "setup_compare.h"
#include "culling.inc"

BOOL DoorShadowBuildPreview(const SetupFile *setup, DWORD index, float scale,
    BgVertex vertices[18], unsigned short *tag, BgRenderFlags *flags, const char **reason)
{ assert(0 && "No door shadows in this placement fixture"); return FALSE; }

static const char *why, *dir;
extern int groupFailAfter;
static void Require(BOOL ok) { if (!ok) { fprintf(stderr, "%s\n", why); assert(ok); } }
static void Near(double a, double b) { if (fabs(a-b) > .04) fprintf(stderr, "%g != %g\n", a, b); assert(fabs(a-b) <= .04); }
static void Center(const SetupObjectGeometry *geometry, DWORD id, double center[3])
{
    double min[3] = {DBL_MAX,DBL_MAX,DBL_MAX}, max[3] = {-DBL_MAX,-DBL_MAX,-DBL_MAX};
    for (DWORD t=0;t<geometry->tricount;t++) if (geometry->objectindices[t] == id)
        for (int c=0;c<3;c++)
        {
            const BgVertex *v = geometry->tris+t*3+c;
            double p[3] = {v->x,v->y,v->z};
            for (int a=0;a<3;a++) { min[a]=fmin(min[a],p[a]); max[a]=fmax(max[a],p[a]); }
        }
    for (int a=0;a<3;a++) center[a]=(min[a]+max[a])*.5;
}
static void CheckPose(const SetupObjectGeometry *before, const SetupObjectGeometry *after,
    int mode, const Rotation *rotation, const Scaling *scale, const Rotation axes[3],
    const double centers[3][3], const double offset[3])
{
    assert(before->tricount == after->tricount);
    for (DWORD t=0;t<before->tricount;t++)
    {
        DWORD id=before->objectindices[t]; assert(id==after->objectindices[t]);
        for (int c=0;c<3;c++)
        {
            const BgVertex *v=before->tris+t*3+c, *w=after->tris+t*3+c;
            double p[3]={v->x,v->y,v->z}, expected[3];
            memcpy(expected,p,sizeof(p));
            if (id<3)
            {
                if (mode==0) for (int a=0;a<3;a++) expected[a]+=offset[a];
                else if (mode==1) RotationPoint(rotation,scale->pivot,p,expected);
                else
                {
                    /* Independent local-coordinate calculation, plus world spacing. */
                    double moved[3], local[3]={0}; ScalingPoint(scale,centers[id],moved);
                    for (int a=0;a<3;a++) for (int b=0;b<3;b++)
                        local[a]+=axes[id].m[b][a]*(p[b]-centers[id][b]);
                    for (int a=0;a<3;a++) local[a]*=scale->factor[a];
                    RotationVector(axes+id,local,expected);
                    for (int a=0;a<3;a++) expected[a]+=moved[a];
                }
            }
            Near(w->x,expected[0]); Near(w->y,expected[1]); Near(w->z,expected[2]);
        }
    }
}
static void Transform(int mode, float levelscale)
{
    SetupFile setup={0}, baseline={0}, saved={0};
    SetupObjectGeometry before={0}, after={0}, reloaded={0};
    EditHistory history={0}; EditHistoryTransaction tx={0}; EditHistoryAsset asset;
    BgDocument bg={0}; StanFile stan={0};
    DWORD ids[]={0,1,2}; Rotation rotation, axes[3];
    double centers[3][3], offset[3]={17,23,-31};
    Scaling scale={.factor={1.5,.7,2}};
    RotationAxis(&scale.axes,0,0); RotationEuler(&rotation,(double[3]){17,35,-9});
    Require(SetupLoadProjectFile(dir,"UsetupgroupZ",&setup,&why));
    /* Two props start on the same pad as an unselected third prop. */
    for (DWORD i=1;i<3;i++)
    {
        SetupPadRef ref; BOOL changed;
        Require(SetupFileTranslateModel(&setup,i,levelscale,(double[3]){80.0*i,30.0*i,-40.0*i},&why));
        Require(SetupFileGetModelPad(&setup,i,&ref));
        RotationAxis(&axes[i],1,37*i);
        Require(SetupFileRotatePad(&setup,&ref,&axes[i],&changed,&why));
    }
    Require(ObjectLoadSetupGeometry(dir,&setup,NULL,levelscale,&before,&why));
    for (DWORD i=0;i<3;i++)
    {
        SetupPadRef ref;
        Center(&before,i,centers[i]); Require(SetupFileGetModelPad(&setup,i,&ref));
        Require(SetupFilePadRotation(&setup,&ref,axes+i));
        for (int a=0;a<3;a++) scale.pivot[a]+=centers[i][a]/3;
    }
    if (mode==3) for (int a=0;a<3;a++) scale.factor[a]=1.75;
    Require(SetupFileClone(&setup,&baseline,&why));
    EditHistoryReset(&history,NULL,&setup,NULL);
    Require(EditHistoryBeginSetupEdit(&history,&setup,"Transform Objects",&tx,&why));
    Require(ObjectTransformSetupModels(dir,&setup,NULL,levelscale,&before,ids,3,
        mode==0 ? offset : NULL,mode==1 ? &rotation : NULL,scale.pivot,mode>=2 ? &scale : NULL,&after,&why));
    CheckPose(&before,&after,mode,&rotation,&scale,axes,centers,offset);
    Require(EditHistoryCommitEdit(&history,NULL,&setup,NULL,&tx,&why)); assert(history.undocount==1);
    Require(EditHistoryUndo(&history,&bg,&setup,&stan,&asset,&why)); SetupAssertNativeEqual(&setup,&baseline);
    Require(EditHistoryRedo(&history,&bg,&setup,&stan,&asset,&why));
    Require(SetupSaveProjectFile(dir,&setup,&why));
    Require(SetupLoadProjectFile(dir,setup.name,&saved,&why));
    Require(ObjectLoadSetupGeometry(dir,&saved,NULL,levelscale,&reloaded,&why));
    CheckPose(&before,&reloaded,mode,&rotation,&scale,axes,centers,offset);
    /* A later invalid member must leave the first member untouched. */
    Require(EditHistoryBeginSetupEdit(&history,&setup,"Invalid Group",&tx,&why));
    DWORD invalid[]={0,999999}; SetupObjectGeometry rejected={0};
    assert(!ObjectTransformSetupModels(dir,&setup,NULL,levelscale,&reloaded,invalid,2,offset,NULL,NULL,NULL,&rejected,&why));
    SetupAssertNativeEqual(&setup,&saved);
    EditHistoryRollbackEdit(&tx,NULL,&setup,NULL); EditHistoryCancelEdit(&tx);
    /* Failure after the first member moved must roll the entire edit back. */
    Require(EditHistoryBeginSetupEdit(&history,&setup,"Failed Group",&tx,&why));
    groupFailAfter=2;
    assert(!ObjectTransformSetupModels(dir,&setup,NULL,levelscale,&reloaded,ids,3,offset,NULL,NULL,NULL,&rejected,&why));
    groupFailAfter=-1;
    assert(!rejected.tris && (setup.size!=saved.size || memcmp(setup.data,saved.data,setup.size)));
    EditHistoryRollbackEdit(&tx,NULL,&setup,NULL); EditHistoryCancelEdit(&tx);
    SetupAssertNativeEqual(&setup,&saved); assert(history.undocount==1);
    Require(SetupSaveProjectFile(dir,&baseline,&why));
    ObjectGeometryFree(&before); ObjectGeometryFree(&after); ObjectGeometryFree(&reloaded);
    EditHistoryFree(&history); SetupFileFree(&setup); SetupFileFree(&baseline); SetupFileFree(&saved);
}
int main(int argc, char **argv)
{
    assert(argc==2); dir=argv[1];
    for (int s=0;s<2;s++) for (int mode=0;mode<4;mode++) Transform(mode,s ? .53931433f : 1);
    puts("PASS: group translation, XYZ rotation, nonuniform/uniform scaling, differently oriented props/doors, shared pads, unselected props, native level scales, one-step undo/redo, save/reload and complete rollback after a later member fails.");
    return 0;
}
