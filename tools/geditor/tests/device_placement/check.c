#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <string.h>
#include "setupload.h"
#include "bghistory.h"
#include "modelload.h"
#include <src/propconstants.h>

void BgDocumentFree(BgDocument *d) { memset(d, 0, sizeof(*d)); }
void StanFileFree(StanFile *d) { memset(d, 0, sizeof(*d)); }
static DWORD Read(const unsigned char *p)
{ return ((DWORD)p[0] << 24) | ((DWORD)p[1] << 16) | ((DWORD)p[2] << 8) | p[3]; }
static void Near(double a, double b) { assert(fabs(a-b) < .0005); }
static void Same(const SetupFile *a, const SetupFile *b)
{
    assert(a->size == b->size && a->objectcount == b->objectcount && a->padcount == b->padcount
        && a->boundpadcount == b->boundpadcount && a->charactercount == b->charactercount);
    assert(!memcmp(a->data, b->data, a->size));
}
static void RoundTrip(const char *dir, const SetupFile *setup)
{
    SetupFile saved = {0}; const char *why;
    /* These are the same native .set bytes taken by ROM export. */
    assert(SetupSaveProjectFile(dir, setup, &why));
    assert(SetupLoadProjectFile(dir, setup->name, &saved, &why));
    Same(setup, &saved); assert(!saved.dirty); SetupFileFree(&saved);
}
static int ModelId(const char *wanted)
{
    const char *name;
    for (int i=0; ModelGetPropDefinition(i,&name,NULL); i++)
    { if (!strcmp(name,wanted)) { return i; } }
    assert(0); return -1;
}

typedef BOOL (*AddDevice)(SetupFile *, int, float, const double *, const double *, DWORD *, const char **);
static void Place(const char *dir, const SetupFile *source, BOOL cctv, float scale,
                  const double facing[3], BOOL mp)
{
    SetupFile setup={0}, before={0}, placed={0}; const char *why;
    EditHistory history={0}; EditHistoryTransaction tx; EditHistoryAsset asset;
    BgDocument bg={0}; StanFile stan={0}; SetupObjectProperties properties;
    double position[3]={300, 180, -400}; DWORD selection, target;
    int model=ModelId(cctv ? SETUP_DEFAULT_CCTV_MODEL : SETUP_DEFAULT_ALARM_MODEL);
    AddDevice add=cctv ? SetupFileAddCctv : SetupFileAddAlarm;
    assert(SetupFileClone(source,&setup,&why));
    if (mp) { strcpy(setup.name,"Ump_setupdevicesZ"); }
    assert(SetupFileClone(&setup,&before,&why));
    EditHistoryReset(&history,&bg,&setup,&stan);
    assert(EditHistoryBeginSetupEdit(&history,&setup,cctv ? "Add CCTV Camera" : "Add Alarm",&tx,&why));
    assert(add(&setup,model,scale,position,facing,&selection,&why));
    assert(selection==source->objectcount && setup.objectcount==source->objectcount+1);
    assert(setup.padcount==source->padcount+(cctv ? 2 : 1));
    assert(setup.boundpadcount==source->boundpadcount && setup.charactercount==source->charactercount && setup.dirty);
    for (int at=0;at<40;at+=4)
    { if (at!=12 && at!=24) { assert(Read(setup.data+at)==Read(source->data+at)); } }
    assert(!memcmp(source->data+40,setup.data+40,source->size-40));
    assert(SetupObjectRelativeTarget(&setup,setup.objects[0].sourceoffset,2,&target) && target==1);
    assert(SetupObjectRelativeTarget(&setup,setup.objects[1].sourceoffset,-2,&target) && target==0);
    assert(SetupFileGetObjectProperties(&setup,selection,&properties,&why));
    assert(properties.object.type==(cctv ? PROPDEF_CCTV : PROPDEF_ALARM));
    assert(properties.object.modelid==model && properties.object.pad==(short)source->padcount);
    assert(properties.object.extrascale==256 && properties.object.flags==PROPFLAG_ONSIDE && !properties.object.flags2);
    Near(properties.health,1000);
    DWORD record=properties.object.sourceoffset;
    assert(Read(setup.data+record+(cctv ? 236 : 128))==48);
    for (DWORD at=16;at<(cctv ? 236u : 128u);at+=4)
    {
        if (at==0x74 || (cctv && (at==0x80 || at==0xcc || at==0xd0 || at==0xdc))) { continue; }
        assert(!Read(setup.data+record+at)); /* Runtime matrices/timers/conversion flags stay zero. */
    }
    const SetupPad *pad=&setup.pads[source->padcount];
    for (int axis=0;axis<3;axis++) { Near(pad->pos[axis]/scale,position[axis]); }
    Near(pad->look[1],1); Near(pad->up[1],0);
    double horizontal=hypot(facing[0],facing[2]);
    if (horizontal>1e-8) { Near(pad->up[0]*facing[0]+pad->up[2]*facing[2],-horizontal); }
    else { Near(pad->up[2],1); }
    for (DWORD i=source->padcount;i<setup.padcount;i++)
    {
        DWORD link=Read(setup.data+Read(setup.data+24)+i*44+36);
        assert(link && link<setup.size && !setup.data[link] && !setup.pads[i].stanname[0]);
    }
    assert(!Read(setup.data+Read(setup.data+24)+setup.padcount*44+36));
    if (cctv)
    {
        assert(properties.cctv.lookpad==(LONG)source->padcount+1);
        const SetupPad *aim=&setup.pads[properties.cctv.lookpad];
        for (int axis=0;axis<3;axis++)
        { Near((aim->pos[axis]-pad->pos[axis])/scale,pad->up[axis]*200); }
        Near(properties.cctv.sweepmin,-45); Near(properties.cctv.sweepmax,45);
        assert(properties.cctv.speed>29.9 && properties.cctv.speed<30.1 && !properties.cctv.range);
    }
    assert(EditHistoryCommitEdit(&history,&bg,&setup,&stan,&tx,&why));
    RoundTrip(dir,&setup); EditHistoryMarkSetupSaved(&history,&setup);
    assert(SetupFileClone(&setup,&placed,&why));
    assert(EditHistoryUndo(&history,&bg,&setup,&stan,&asset,&why) && setup.dirty); Same(&setup,&before);
    assert(EditHistoryRedo(&history,&bg,&setup,&stan,&asset,&why) && !setup.dirty); Same(&setup,&placed);
    /* Retargeting, moving and rotating a newly authored device must retain
     * the native type and leave the earlier objects' pads/links intact. */
    BOOL changed; SetupObjectPropertyEdit edit={0}; SetupPadRef ref;
    edit.objectindex=selection; edit.sourceoffset=setup.objects[selection].sourceoffset;
    edit.type=properties.object.type; edit.property=cctv ? SETUP_OBJECT_CCTV_LOOK_PAD : SETUP_OBJECT_HEALTH;
    edit.value=cctv ? 0 : 250;
    assert(SetupFileSetObjectProperty(&setup,&edit,&changed,&why) && changed);
    const double offset[3]={10,20,30}; Rotation rotation; RotationAxis(&rotation,1,.5);
    assert(SetupFileTranslateModel(&setup,selection,scale,offset,&why));
    assert(SetupFileGetModelPad(&setup,selection,&ref));
    assert(SetupFileRotatePad(&setup,&ref,&rotation,&changed,&why) && changed);
    assert(!memcmp(source->pads,setup.pads,sizeof(*source->pads)*source->padcount));
    RoundTrip(dir,&setup);
    DWORD second;
    assert(add(&setup,model,scale,position,facing,&second,&why) && second==selection+1);
    assert(setup.objects[second].pad!=setup.objects[selection].pad);
    assert(SetupFileDeleteObject(&setup,second,&why) && setup.objects[second].deleted);
    RoundTrip(dir,&setup);
    SetupFileFree(&setup); SetupFileFree(&before); SetupFileFree(&placed); EditHistoryFree(&history);
}

int main(int argc,char **argv)
{
    SetupFile source={0},setup={0}; const char *why; DWORD selection;
    assert(argc==2 && SetupLoadProjectFile(argv[1],"UsetupdevicesZ",&source,&why));
    const float scales[]={.15019713f,.53931433f,1.20648f,1};
    const double facing[][3]={{0,0,-1},{1,.5,1},{-1,0,0},{0,-1,0}};
    for (int kind=0;kind<2;kind++) for (int mp=0;mp<2;mp++)
        for (int s=0;s<4;s++) for (int f=0;f<4;f++) { Place(argv[1],&source,kind,scales[s],facing[f],mp); }
    puts("PASS: solo/MP CCTV and alarms, native record sizes, level scales, mounted orientation, private look-at pads and existing references.");
    puts("PASS: save/reload, undo/redo, retargeting, health, transforms, repeated placement and deletion.");
    const double pos[3]={0,0,0},bad[3]={NAN,0,0};
    for (int kind=0;kind<2;kind++)
    {
        AddDevice add=kind ? SetupFileAddCctv : SetupFileAddAlarm;
        int model=ModelId(kind ? SETUP_DEFAULT_CCTV_MODEL : SETUP_DEFAULT_ALARM_MODEL);
        assert(SetupFileClone(&source,&setup,&why));
        assert(!add(&setup,model,0,pos,facing[0],&selection,&why)); Same(&setup,&source);
        assert(!add(&setup,model,1,bad,facing[0],&selection,&why)); Same(&setup,&source);
        assert(!add(&setup,model,1,pos,bad,&selection,&why)); Same(&setup,&source);
        assert(!add(&setup,-1,1,pos,facing[0],&selection,&why)); Same(&setup,&source);
        assert(!add(&setup,model,1,NULL,facing[0],&selection,&why)); Same(&setup,&source);
        assert(!add(&setup,model,1,pos,NULL,&selection,&why)); Same(&setup,&source);
        assert(!add(&setup,model,1,pos,facing[0],NULL,&why)); Same(&setup,&source);
        DWORD count=setup.padcount;
        setup.padcount=kind ? 9999 : 10000;
        assert(!add(&setup,model,1,pos,facing[0],&selection,&why)); setup.padcount=count; Same(&setup,&source);
        assert(!setup.dirty); SetupFileFree(&setup);
        assert(SetupLoadProjectFile(argv[1],"UsetupemptyZ",&setup,&why));
        assert(!setup.objectcount && !setup.padcount);
        assert(add(&setup,model,1,pos,facing[0],&selection,&why) && selection==0 && setup.objects[0].pad==0);
        assert(setup.padcount==(kind ? 2u : 1u));
        SetupFileFree(&setup);
    }
    SetupFileFree(&source);
    puts("PASS: invalid drops/pad exhaustion are atomic; empty lists can gain their first camera or alarm.");
    return 0;
}
