#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <string.h>
#include "setupload.h"
#include "setup_compare.h"
#include "bghistory.h"
#include "modelload.h"
#include <src/propconstants.h>

void BgDocumentFree(BgDocument *d) { memset(d,0,sizeof(*d)); }
void StanFileFree(StanFile *d) { memset(d,0,sizeof(*d)); }
static DWORD Read(const unsigned char *p)
{ return (DWORD)p[0]<<24 | (DWORD)p[1]<<16 | (DWORD)p[2]<<8 | p[3]; }
static int Model(const char *wanted)
{
    const char *name;
    for (int i=0; ModelGetPropDefinition(i,&name,NULL); i++)
    { if (!strcmp(wanted,name)) return i; }
    assert(0); return -1;
}
static void RoundTrip(const char *dir, const SetupFile *setup, DWORD index, int model, DWORD type)
{
    SetupFile loaded={0}; SetupObjectProperties p; const char *why;
    assert(SetupSaveProjectFile(dir,setup,&why) && SetupLoadProjectFile(dir,setup->name,&loaded,&why));
    SetupAssertNativeEqual(setup,&loaded);
    assert(SetupFileGetObjectProperties(&loaded,index,&p,&why));
    assert(p.object.type==PROPDEF_MAGAZINE && p.object.modelid==model && p.ammotype==type);
    SetupFileFree(&loaded);
}
static void Place(const char *dir, const SetupFile *source, float scale, BOOL mp)
{
    SetupFile setup={0}, before={0}, placed={0}; SetupObjectProperties p;
    EditHistory history={0}; EditHistoryTransaction tx; BgDocument bg={0}; StanFile stan={0};
    const char *why; DWORD selection,target; BOOL changed;
    const double pos[3]={300,180,-400}; int model=Model(SETUP_DEFAULT_AMMO_MODEL);
    assert(SetupFileClone(source,&setup,&why));
    if (mp) { strcpy(setup.name,"Ump_setupammoZ"); }
    assert(SetupFileClone(&setup,&before,&why));
    EditHistoryReset(&history,&bg,&setup,&stan);
    assert(EditHistoryBeginSetupEdit(&history,&setup,"Add Ammo",&tx,&why));
    assert(SetupFileAddAmmo(&setup,model,scale,pos,&selection,&why));
    assert(selection==source->objectcount && setup.objectcount==source->objectcount+1);
    assert(setup.padcount==source->padcount+1 && setup.boundpadcount==source->boundpadcount);
    assert(setup.charactercount==source->charactercount);
    assert(SetupFileGetObjectProperties(&setup,selection,&p,&why));
    assert(p.object.type==PROPDEF_MAGAZINE && p.object.modelid==model && p.ammotype==AMMO_9MM);
    assert(p.object.extrascale==256 && p.object.flags==PROPFLAG_ALLOWFALL && !p.object.flags2);
    assert(Read(setup.data+p.object.sourceoffset+132)==PROPDEF_END); /* Native 33-word record. */
    for (int a=0;a<3;a++) { assert(fabs(setup.pads[p.object.pad].pos[a]/scale-pos[a])<.0005); }
    assert(!memcmp(source->data+40,setup.data+40,source->size-40));
    assert(SetupObjectRelativeTarget(&setup,setup.objects[0].sourceoffset,2,&target) && target==1);
    assert(SetupObjectRelativeTarget(&setup,setup.objects[1].sourceoffset,-2,&target) && target==0);
    assert(EditHistoryCommitEdit(&history,&bg,&setup,&stan,&tx,&why));
    assert(SetupFileClone(&setup,&placed,&why));
    RoundTrip(dir,&setup,selection,model,AMMO_9MM);
    assert(EditHistoryUndo(&history,&bg,&setup,&stan,NULL,&why)); SetupAssertNativeEqual(&before,&setup);
    assert(EditHistoryRedo(&history,&bg,&setup,&stan,NULL,&why)); SetupAssertNativeEqual(&placed,&setup);

    assert(EditHistoryBeginSetupEdit(&history,&setup,"Edit Ammo",&tx,&why));
    SetupObjectPropertyEdit edit={0};
    edit.objectindex=selection; edit.sourceoffset=setup.objects[selection].sourceoffset;
    edit.type=PROPDEF_MAGAZINE; edit.property=SETUP_OBJECT_AMMO_TYPE; edit.value=AMMO_ROCKETS;
    assert(SetupFileSetObjectProperty(&setup,&edit,&changed,&why) && changed);
    edit.property=SETUP_OBJECT_MODEL; edit.value=Model("Pammo_crate1Z");
    assert(SetupFileSetObjectProperty(&setup,&edit,&changed,&why) && changed);
    assert(EditHistoryCommitEdit(&history,&bg,&setup,&stan,&tx,&why));
    RoundTrip(dir,&setup,selection,(int)edit.value,AMMO_ROCKETS);
    assert(EditHistoryUndo(&history,&bg,&setup,&stan,NULL,&why)); SetupAssertNativeEqual(&placed,&setup);
    assert(EditHistoryRedo(&history,&bg,&setup,&stan,NULL,&why));
    RoundTrip(dir,&setup,selection,(int)edit.value,AMMO_ROCKETS);
    DWORD second;
    assert(SetupFileAddAmmo(&setup,model,scale,pos,&second,&why) && second!=selection);
    assert(setup.objects[selection].pad!=setup.objects[second].pad);
    assert(SetupFileDeleteObject(&setup,second,&why));
    RoundTrip(dir,&setup,selection,(int)edit.value,AMMO_ROCKETS);
    SetupFileFree(&setup); SetupFileFree(&before); SetupFileFree(&placed); EditHistoryFree(&history);
}
int main(int argc, char **argv)
{
    SetupFile source={0}; const char *why; assert(argc==2);
    assert(SetupLoadProjectFile(argv[1],"UsetupammoZ",&source,&why));
    const float scales[]={.15019713f,.53931433f,1};
    for (int mp=0;mp<2;mp++) for (unsigned i=0;i<sizeof(scales)/sizeof(*scales);i++)
    { Place(argv[1],&source,scales[i],mp); }
    SetupFileFree(&source);
    puts("PASS: native 9mm pickup, editable model/ammo type, solo/MP, private pads and scales, retained setup links, save/reload, undo/redo, repeated placement and deletion.");
    return 0;
}
