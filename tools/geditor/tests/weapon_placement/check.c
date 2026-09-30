#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <string.h>
#include "setupload.h"
#include "setup_compare.h"
#include "bghistory.h"
#include "modelload.h"
#include <src/propconstants.h>
#include "weaponchoices.h"

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
    assert(p.object.type==PROPDEF_COLLECTABLE && p.object.modelid==model && p.weapontype==type);
    SetupFileFree(&loaded);
}
static void Place(const char *dir, const SetupFile *source, float scale, BOOL mp)
{
    SetupFile setup={0}, before={0}, placed={0}; SetupObjectProperties p;
    EditHistory history={0}; EditHistoryTransaction tx; BgDocument bg={0}; StanFile stan={0};
    const char *why; DWORD selection,target; BOOL changed;
    const double pos[3]={300,180,-400}; int model=Model(SETUP_DEFAULT_WEAPON_MODEL);
    assert(SetupFileClone(source,&setup,&why));
    if (mp) { strcpy(setup.name,"Ump_setupweaponZ"); }
    assert(SetupFileClone(&setup,&before,&why));
    EditHistoryReset(&history,&bg,&setup,&stan);
    assert(EditHistoryBeginSetupEdit(&history,&setup,"Add Weapon",&tx,&why));
    assert(SetupFileAddWeapon(&setup,model,scale,pos,&selection,&why));
    assert(selection==source->objectcount && setup.objectcount==source->objectcount+1);
    assert(setup.padcount==source->padcount+1 && setup.boundpadcount==source->boundpadcount);
    assert(setup.charactercount==source->charactercount);
    assert(SetupFileGetObjectProperties(&setup,selection,&p,&why));
    assert(p.object.type==PROPDEF_COLLECTABLE && p.object.modelid==model && p.weapontype==SETUP_DEFAULT_WEAPON_ITEM);
    assert(p.object.extrascale==256 && p.object.flags==PROPFLAG_ALLOWFALL && !p.object.flags2);
    assert(Read(setup.data+p.object.sourceoffset+136)==PROPDEF_END); /* Native 34-word record. */
    assert(Read(setup.data+p.object.sourceoffset+0x80)==((DWORD)SETUP_DEFAULT_WEAPON_ITEM<<24 | 0xffffffu));
    assert(Read(setup.data+p.object.sourceoffset+0x84)==0);
    assert(model==SetupWeaponChoiceForItem(SETUP_DEFAULT_WEAPON_ITEM)->model);
    for (int a=0;a<3;a++) { assert(fabs(setup.pads[p.object.pad].pos[a]/scale-pos[a])<.0005); }
    assert(!memcmp(source->data+40,setup.data+40,source->size-40));
    assert(SetupObjectRelativeTarget(&setup,setup.objects[0].sourceoffset,2,&target) && target==1);
    assert(SetupObjectRelativeTarget(&setup,setup.objects[1].sourceoffset,-2,&target) && target==0);
    assert(EditHistoryCommitEdit(&history,&bg,&setup,&stan,&tx,&why));
    assert(SetupFileClone(&setup,&placed,&why));
    RoundTrip(dir,&setup,selection,model,SETUP_DEFAULT_WEAPON_ITEM);
    assert(EditHistoryUndo(&history,&bg,&setup,&stan,NULL,&why)); SetupAssertNativeEqual(&before,&setup);
    assert(EditHistoryRedo(&history,&bg,&setup,&stan,NULL,&why)); SetupAssertNativeEqual(&placed,&setup);

    const SetupWeaponChoice *choices; DWORD count;
    choices=SetupWeaponChoices(&count);
    SetupObjectPropertyEdit edit={0};
    edit.objectindex=selection; edit.type=PROPDEF_COLLECTABLE; edit.property=SETUP_OBJECT_WEAPON_TYPE;
    for (DWORD i=0;i<count;i++)
    {
        if (choices[i].item<0 || choices[i].item==SETUP_DEFAULT_WEAPON_ITEM) continue;
        assert(EditHistoryBeginSetupEdit(&history,&setup,"Change Weapon Type",&tx,&why));
        edit.sourceoffset=setup.objects[selection].sourceoffset; edit.value=choices[i].item;
        unsigned char record[136]; memcpy(record,setup.data+edit.sourceoffset,sizeof(record));
        assert(SetupFileSetObjectProperty(&setup,&edit,&changed,&why) && changed);
        const unsigned char *after=setup.data+edit.sourceoffset;
        for (int byte=0;byte<136;byte++)
            if (byte!=4 && byte!=5 && byte!=0x80) assert(record[byte]==after[byte]);
        assert(!SetupFileSetObjectProperty(&setup,&(SetupObjectPropertyEdit){.objectindex=0,
            .sourceoffset=setup.objects[0].sourceoffset,.type=setup.objects[0].type,
            .property=SETUP_OBJECT_WEAPON_TYPE,.value=choices[i].item},&changed,&why));
        assert(SetupFileSetObjectProperty(&setup,&edit,&changed,&why) && !changed);
        assert(EditHistoryCommitEdit(&history,&bg,&setup,&stan,&tx,&why));
        RoundTrip(dir,&setup,selection,choices[i].model,choices[i].item);
        assert(EditHistoryUndo(&history,&bg,&setup,&stan,NULL,&why)); SetupAssertNativeEqual(&placed,&setup);
        assert(EditHistoryRedo(&history,&bg,&setup,&stan,NULL,&why));
        RoundTrip(dir,&setup,selection,choices[i].model,choices[i].item);
        assert(EditHistoryUndo(&history,&bg,&setup,&stan,NULL,&why));
    }
    const double invalid[]={-1,0,1,4.5,127,240,256,INFINITY,NAN};
    edit.sourceoffset=setup.objects[selection].sourceoffset;
    for (unsigned i=0;i<sizeof(invalid)/sizeof(*invalid);i++)
    {
        edit.value=invalid[i];
        assert(!SetupFileSetObjectProperty(&setup,&edit,&changed,&why) && !changed);
        SetupAssertNativeEqual(&placed,&setup);
    }
    edit.value=8; edit.sourceoffset++;
    assert(!SetupFileSetObjectProperty(&setup,&edit,&changed,&why) && !changed);
    SetupAssertNativeEqual(&placed,&setup);
    DWORD second;
    assert(SetupFileAddWeapon(&setup,model,scale,pos,&second,&why) && second!=selection);
    assert(setup.objects[selection].pad!=setup.objects[second].pad);
    assert(SetupFileDeleteObject(&setup,second,&why));
    RoundTrip(dir,&setup,selection,model,SETUP_DEFAULT_WEAPON_ITEM);
    SetupFileFree(&setup); SetupFileFree(&before); SetupFileFree(&placed); EditHistoryFree(&history);
}
int main(int argc, char **argv)
{
    SetupFile source={0}; const char *why; assert(argc==2);
    assert(SetupLoadProjectFile(argv[1],"UsetupweaponZ",&source,&why));
    const float scales[]={.15019713f,.53931433f,1};
    for (int mp=0;mp<2;mp++) for (unsigned i=0;i<sizeof(scales)/sizeof(*scales);i++)
    { Place(argv[1],&source,scales[i],mp); }
    SetupFileFree(&source);
    puts("PASS: native PP7 pickup and inactive timer, all weapon/model choices, invalid/stale edits, solo/MP, private pads and scales, retained setup links, save/reload, undo/redo, repeated placement and deletion.");
    return 0;
}
